
/*============================================================================

    This file is part of FLINT.

    FLINT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    FLINT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FLINT; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

===============================================================================*/
/****************************************************************************

   Copyright (C) 2010 Daniel Woodhouse
   
*****************************************************************************/

/* 
   This function is developed for Tom Coates and Alessio Corti 
   at Imperial College 
*/

#include <mpir.h>
#include <stdlib.h>
#include <math.h>
#include "flint.h"
#include "ulong_extras.h"
#include "nmod_mpoly.h"
#include "fmpz.h"
#include "longlong.h"
#include "nmod_vec.h"

void get_period_sequence(fmpz_t *zeroCoefficients, 
          long *coefficients, ulong *exponents, ulong length, 
       ulong monomial, int pow, ulong *primes, int numOfPrimes, ulong nvars)
{  
   ulong * cInverses = (ulong *)malloc(sizeof(ulong)*(numOfPrimes -1)); 
   fmpz_t temp, temp1, temp2, m1, m1_temp, res;
   nmod_mpoly_t poly, polyTotal, tempPoly;

   int p, i, j, k;
   nmod_t mod;   
   long coeffTemp;
   ulong ebits;
   ulong ** residues;
   
   fmpz_init(temp);
   fmpz_init(temp1);
   fmpz_init(temp2);
   
   fmpz_init(res);
   
   fmpz_init(m1);
   fmpz_init(m1_temp);
   
   /*Calculate the inverses of our co-prime pairs */
   fmpz_set_ui(temp1, primes[0]);
   
   for (p = 0; p < numOfPrimes - 1; p++)
   {   
      fmpz_set_ui(temp2, primes[p+1]);
      fmpz_invmod(temp, temp1, temp2);
      cInverses[p] = fmpz_get_ui(temp);
   
      fmpz_mul(temp, temp1, temp2);
      
      fmpz_set(temp1, temp);
      
   }
 
   residues = (ulong **) malloc(numOfPrimes * sizeof(ulong*));
   for (i = 0; i < numOfPrimes; i++)
      residues[i] = (ulong *) malloc(pow * sizeof(ulong));
 
   ebits = FLINT_BITS / nvars;
   
   /* 
      for each prime take the power adding each 'zero coefficient' 
      to our array
   */
   for (i = 0; i < numOfPrimes; i++)
   {
      long halfway;
      nmod_mpoly_t * first_half;

      nmod_mpoly_init2(poly, primes[i], length, (long) nvars, (ulong) ebits);
      nmod_mpoly_init2(polyTotal, primes[i], length, (long) nvars, (ulong) ebits);
      nmod_mpoly_init2(tempPoly, primes[i], length, (long) nvars, (ulong) ebits);
      
      if(pow % 2 == 0)
         halfway = pow / 2;
      else
         halfway = (pow / 2) + 1;

      /* 
         here I need to initialise and array of nmod_mpolys_t 
         of length ceil(pow/2)
      */
      first_half = malloc(sizeof(nmod_mpoly_t) * halfway);
      
      nmod_init(&mod, primes[i]);
      
      for (k = 0; k < length; k++)
      {
          if(coefficients[k] < 0)
          {
             coeffTemp = coefficients[k];
             while(coeffTemp <0)
                coeffTemp = coeffTemp + (long)primes[i];
             poly->coeffs[k] = (ulong) coeffTemp;
          }
          else
             NMOD_RED(poly->coeffs[k], coefficients[k], mod);

          poly->exps[k] = exponents[k];
      }
         
      poly->length = length;
      
      nmod_mpoly_set(polyTotal, poly); 
      residues[i][0] = nmod_mpoly_get_coeff(polyTotal,  monomial );
      nmod_mpoly_init2(first_half[0], primes[i], length, (long) nvars, (ulong) ebits);
      nmod_mpoly_set(first_half[0], poly);
      
      /* now do the first half */
      for(j=1; j<halfway; j++)
      {        
         nmod_mpoly_mul_heap(tempPoly, polyTotal, poly);
         nmod_mpoly_set(polyTotal, tempPoly);
         nmod_mpoly_init2(first_half[j], primes[i], length, 
                                    (long) nvars, (ulong) ebits);
         nmod_mpoly_set(first_half[j], tempPoly);
         residues[i][j] = nmod_mpoly_get_coeff(polyTotal, (j+1)*monomial);   
      }
      
      /* now do the next half using the polys already generated. */
      for(j = halfway; j <pow; j++)
      {   
         if (j % 2 == 0)
            residues[i][j] = nmod_mpoly_get_coeff_of_product(first_half[j/2 - 1], first_half[j/2], (j+1)*(monomial));
         else if(j%2 == 1)
            residues[i][j] = nmod_mpoly_get_coeff_of_product(first_half[j/2], first_half[j/2], (j+1)*(monomial));
      }

      /* 
         Need to clear all of the polynomials before I can clear
         the whole array.
      */
      for (j = 1; j < halfway; j++)        
         nmod_mpoly_clear(first_half[j]);

      free(first_half);
   }
   
   nmod_mpoly_clear(poly);
   nmod_mpoly_clear(polyTotal);
   nmod_mpoly_clear(tempPoly);
  
   for (i = 0; i < pow; i++)
      fmpz_init(zeroCoefficients[i]);
  
   /* for each set of residues power j+2 */
   for(j = 0; j < pow; j++)
   { 
      fmpz_set_ui(res, residues[0][j]);
      fmpz_set_ui(m1, primes[0]);

      for (i = 1; i < numOfPrimes; i++)
      {  
         double pre = n_precompute_inverse(primes[i]);

         fmpz_t inv;
         fmpz_t mm2;
         fmpz_t out;
         
         fmpz_init(out);
         fmpz_init(inv);
         fmpz_init(mm2);
         fmpz_set_ui(mm2, primes[i]);
         fmpz_invmod(inv, m1, mm2);
              
         fmpz_CRT_ui_precomp(out, res, m1, residues[i][j], 
                                   primes[i], cInverses[i-1], pre );  
         
         fmpz_set(temp, res);
         fmpz_set(res, out);
            
         fmpz_clear(out);
         fmpz_clear(inv);
         fmpz_clear(mm2);              

         fmpz_mul_ui(m1, m1, primes[i]);
      }
      
      fmpz_set_ui(temp1, (ulong) 2);
      fmpz_fdiv_q(temp, m1, temp1);
      
      if (fmpz_cmpabs(res, temp) > 0)
         fmpz_sub(res, res, m1);

      fmpz_set(zeroCoefficients[j], res);             
   }
   
   for (i = 0; i < numOfPrimes; i++)
      free(residues[i]);
 
   free(residues);
   free(cInverses);
   fmpz_clear(temp);
   fmpz_clear(temp1);
   fmpz_clear(temp2);
   
   fmpz_clear(res); 

   fmpz_clear(m1);
   fmpz_clear(m1_temp);   
}