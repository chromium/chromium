/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include <cmath>
#include <cstdlib>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/geometry/float_box.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/transforms/rotation.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/transform.h"

#if defined(ARCH_CPU_X86_64)
#include <emmintrin.h>
#endif

#if defined(HAVE_MIPS_MSA_INTRINSICS)
#include "third_party/blink/renderer/platform/cpu/mips/common_macros_msa.h"
#endif

namespace blink {

//
// Supporting Math Functions
//
// This is a set of function from various places (attributed inline) to do
// things like inversion and decomposition of a 4x4 matrix. They are used
// throughout the code
//

//
// Adapted from Matrix Inversion by Richard Carling, Graphics Gems
// <http://tog.acm.org/GraphicsGems/index.html>.

// EULA: The Graphics Gems code is copyright-protected. In other words, you
// cannot claim the text of the code as your own and resell it. Using the code
// is permitted in any program, product, or library, non-commercial or
// commercial. Giving credit is not required, though is a nice gesture. The code
// comes as-is, and if there are any flaws or problems with any Gems code,
// nobody involved with Gems - authors, editors, publishers, or webmasters - are
// to be held responsible. Basically, don't be a jerk, and remember that
// anything free comes with no guarantee.

typedef double Vector4[4];
typedef double Vector3[3];

// inverse(original_matrix, inverse_matrix)
//
// calculate the inverse of a 4x4 matrix
//
// -1
// A  = ___1__ adjoint A
//       det A

//  double = determinant2x2(double a, double b, double c, double d)
//
//  calculate the determinant of a 2x2 matrix.

static double Determinant2x2(double a, double b, double c, double d) {
  return a * d - b * c;
}

//  double = determinant3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3)
//
//  Calculate the determinant of a 3x3 matrix
//  in the form
//
//      | a1,  b1,  c1 |
//      | a2,  b2,  c2 |
//      | a3,  b3,  c3 |

static double Determinant3x3(double a1,
                             double a2,
                             double a3,
                             double b1,
                             double b2,
                             double b3,
                             double c1,
                             double c2,
                             double c3) {
  return a1 * Determinant2x2(b2, b3, c2, c3) -
         b1 * Determinant2x2(a2, a3, c2, c3) +
         c1 * Determinant2x2(a2, a3, b2, b3);
}

//  double = determinant4x4(matrix)
//
//  calculate the determinant of a 4x4 matrix.

static double Determinant4x4(const TransformationMatrix::Matrix4& m) {
  // Assign to individual variable names to aid selecting
  // correct elements

  double a1 = m[0][0];
  double b1 = m[0][1];
  double c1 = m[0][2];
  double d1 = m[0][3];

  double a2 = m[1][0];
  double b2 = m[1][1];
  double c2 = m[1][2];
  double d2 = m[1][3];

  double a3 = m[2][0];
  double b3 = m[2][1];
  double c3 = m[2][2];
  double d3 = m[2][3];

  double a4 = m[3][0];
  double b4 = m[3][1];
  double c4 = m[3][2];
  double d4 = m[3][3];

  return a1 * Determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4) -
         b1 * Determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4) +
         c1 * Determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4) -
         d1 * Determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

#if !defined(ARCH_CPU_ARM64) && !HAVE_MIPS_MSA_INTRINSICS
// adjoint( original_matrix, inverse_matrix )
//
//   calculate the adjoint of a 4x4 matrix
//
//    Let  a   denote the minor determinant of matrix A obtained by
//         ij
//
//    deleting the ith row and jth column from A.
//
//                  i+j
//   Let  b   = (-1)    a
//        ij            ji
//
//  The matrix B = (b  ) is the adjoint of A
//                   ij

static inline void Adjoint(const TransformationMatrix::Matrix4& matrix,
                           TransformationMatrix::Matrix4& result) {
  // Assign to individual variable names to aid
  // selecting correct values
  double a1 = matrix[0][0];
  double b1 = matrix[0][1];
  double c1 = matrix[0][2];
  double d1 = matrix[0][3];

  double a2 = matrix[1][0];
  double b2 = matrix[1][1];
  double c2 = matrix[1][2];
  double d2 = matrix[1][3];

  double a3 = matrix[2][0];
  double b3 = matrix[2][1];
  double c3 = matrix[2][2];
  double d3 = matrix[2][3];

  double a4 = matrix[3][0];
  double b4 = matrix[3][1];
  double c4 = matrix[3][2];
  double d4 = matrix[3][3];

  // Row column labeling reversed since we transpose rows & columns
  result[0][0] = Determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
  result[1][0] = -Determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
  result[2][0] = Determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
  result[3][0] = -Determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

  result[0][1] = -Determinant3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
  result[1][1] = Determinant3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
  result[2][1] = -Determinant3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
  result[3][1] = Determinant3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

  result[0][2] = Determinant3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
  result[1][2] = -Determinant3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
  result[2][2] = Determinant3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
  result[3][2] = -Determinant3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

  result[0][3] = -Determinant3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
  result[1][3] = Determinant3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
  result[2][3] = -Determinant3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
  result[3][3] = Determinant3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}
#endif

// Returns false if the matrix is not invertible
static bool Inverse(const TransformationMatrix::Matrix4& matrix,
                    TransformationMatrix::Matrix4& result) {
  // Calculate the 4x4 determinant
  // If the determinant is zero,
  // then the inverse matrix is not unique.
  double det = Determinant4x4(matrix);

  if (det == 0)
    return false;

#if defined(ARCH_CPU_ARM64)
  double rdet = 1 / det;
  const double* mat = &(matrix[0][0]);
  double* pr = &(result[0][0]);
  asm volatile(
      // mat: v16 - v23
      // m11, m12, m13, m14
      // m21, m22, m23, m24
      // m31, m32, m33, m34
      // m41, m42, m43, m44
      "ld1 {v16.2d - v19.2d}, [%[mat]], 64  \n\t"
      "ld1 {v20.2d - v23.2d}, [%[mat]]      \n\t"
      "ins v30.d[0], %[rdet]         \n\t"
      // Determinant: right mat2x2
      "trn1 v0.2d, v17.2d, v21.2d    \n\t"
      "trn2 v1.2d, v19.2d, v23.2d    \n\t"
      "trn2 v2.2d, v17.2d, v21.2d    \n\t"
      "trn1 v3.2d, v19.2d, v23.2d    \n\t"
      "trn2 v5.2d, v21.2d, v23.2d    \n\t"
      "trn1 v4.2d, v17.2d, v19.2d    \n\t"
      "trn2 v6.2d, v17.2d, v19.2d    \n\t"
      "trn1 v7.2d, v21.2d, v23.2d    \n\t"
      "trn2 v25.2d, v23.2d, v21.2d   \n\t"
      "trn1 v27.2d, v23.2d, v21.2d   \n\t"
      "fmul v0.2d, v0.2d, v1.2d      \n\t"
      "fmul v1.2d, v4.2d, v5.2d      \n\t"
      "fmls v0.2d, v2.2d, v3.2d      \n\t"
      "fmul v2.2d, v4.2d, v25.2d     \n\t"
      "fmls v1.2d, v6.2d, v7.2d      \n\t"
      "fmls v2.2d, v6.2d, v27.2d     \n\t"
      // Adjoint:
      // v24: A11A12, v25: A13A14
      // v26: A21A22, v27: A23A24
      "fmul v3.2d, v18.2d, v0.d[1]   \n\t"
      "fmul v4.2d, v16.2d, v0.d[1]   \n\t"
      "fmul v5.2d, v16.2d, v1.d[1]   \n\t"
      "fmul v6.2d, v16.2d, v2.d[1]   \n\t"
      "fmls v3.2d, v20.2d, v1.d[1]   \n\t"
      "fmls v4.2d, v20.2d, v2.d[0]   \n\t"
      "fmls v5.2d, v18.2d, v2.d[0]   \n\t"
      "fmls v6.2d, v18.2d, v1.d[0]   \n\t"
      "fmla v3.2d, v22.2d, v2.d[1]   \n\t"
      "fmla v4.2d, v22.2d, v1.d[0]   \n\t"
      "fmla v5.2d, v22.2d, v0.d[0]   \n\t"
      "fmla v6.2d, v20.2d, v0.d[0]   \n\t"
      "fneg v3.2d, v3.2d             \n\t"
      "fneg v5.2d, v5.2d             \n\t"
      "trn1 v26.2d, v3.2d, v4.2d     \n\t"
      "trn1 v27.2d, v5.2d, v6.2d     \n\t"
      "trn2 v24.2d, v3.2d, v4.2d     \n\t"
      "trn2 v25.2d, v5.2d, v6.2d     \n\t"
      "fneg v24.2d, v24.2d           \n\t"
      "fneg v25.2d, v25.2d           \n\t"
      // Inverse
      // v24: I11I12, v25: I13I14
      // v26: I21I22, v27: I23I24
      "fmul v24.2d, v24.2d, v30.d[0] \n\t"
      "fmul v25.2d, v25.2d, v30.d[0] \n\t"
      "fmul v26.2d, v26.2d, v30.d[0] \n\t"
      "fmul v27.2d, v27.2d, v30.d[0] \n\t"
      "st1 {v24.2d - v27.2d}, [%[pr]], 64 \n\t"
      // Determinant: left mat2x2
      "trn1 v0.2d, v16.2d, v20.2d    \n\t"
      "trn2 v1.2d, v18.2d, v22.2d    \n\t"
      "trn2 v2.2d, v16.2d, v20.2d    \n\t"
      "trn1 v3.2d, v18.2d, v22.2d    \n\t"
      "trn2 v5.2d, v20.2d, v22.2d    \n\t"
      "trn1 v4.2d, v16.2d, v18.2d    \n\t"
      "trn2 v6.2d, v16.2d, v18.2d    \n\t"
      "trn1 v7.2d, v20.2d, v22.2d    \n\t"
      "trn2 v25.2d, v22.2d, v20.2d   \n\t"
      "trn1 v27.2d, v22.2d, v20.2d   \n\t"
      "fmul v0.2d, v0.2d, v1.2d      \n\t"
      "fmul v1.2d, v4.2d, v5.2d      \n\t"
      "fmls v0.2d, v2.2d, v3.2d      \n\t"
      "fmul v2.2d, v4.2d, v25.2d     \n\t"
      "fmls v1.2d, v6.2d, v7.2d      \n\t"
      "fmls v2.2d, v6.2d, v27.2d     \n\t"
      // Adjoint:
      // v24: A31A32, v25: A33A34
      // v26: A41A42, v27: A43A44
      "fmul v3.2d, v19.2d, v0.d[1]   \n\t"
      "fmul v4.2d, v17.2d, v0.d[1]   \n\t"
      "fmul v5.2d, v17.2d, v1.d[1]   \n\t"
      "fmul v6.2d, v17.2d, v2.d[1]   \n\t"
      "fmls v3.2d, v21.2d, v1.d[1]   \n\t"
      "fmls v4.2d, v21.2d, v2.d[0]   \n\t"
      "fmls v5.2d, v19.2d, v2.d[0]   \n\t"
      "fmls v6.2d, v19.2d, v1.d[0]   \n\t"
      "fmla v3.2d, v23.2d, v2.d[1]   \n\t"
      "fmla v4.2d, v23.2d, v1.d[0]   \n\t"
      "fmla v5.2d, v23.2d, v0.d[0]   \n\t"
      "fmla v6.2d, v21.2d, v0.d[0]   \n\t"
      "fneg v3.2d, v3.2d             \n\t"
      "fneg v5.2d, v5.2d             \n\t"
      "trn1 v26.2d, v3.2d, v4.2d     \n\t"
      "trn1 v27.2d, v5.2d, v6.2d     \n\t"
      "trn2 v24.2d, v3.2d, v4.2d     \n\t"
      "trn2 v25.2d, v5.2d, v6.2d     \n\t"
      "fneg v24.2d, v24.2d           \n\t"
      "fneg v25.2d, v25.2d           \n\t"
      // Inverse
      // v24: I31I32, v25: I33I34
      // v26: I41I42, v27: I43I44
      "fmul v24.2d, v24.2d, v30.d[0] \n\t"
      "fmul v25.2d, v25.2d, v30.d[0] \n\t"
      "fmul v26.2d, v26.2d, v30.d[0] \n\t"
      "fmul v27.2d, v27.2d, v30.d[0] \n\t"
      "st1 {v24.2d - v27.2d}, [%[pr]] \n\t"
      : [mat] "+r"(mat), [pr] "+r"(pr)
      : [rdet] "r"(rdet)
      : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16", "v17",
        "v18", "v19", "v20", "v21", "v22", "v23", "24", "25", "v26", "v27",
        "v28", "v29", "v30");
#elif defined(HAVE_MIPS_MSA_INTRINSICS)
  const double rDet = 1 / det;
  const double* mat = &(matrix[0][0]);
  v2f64 mat0, mat1, mat2, mat3, mat4, mat5, mat6, mat7;
  v2f64 rev2, rev3, rev4, rev5, rev6, rev7;
  v2f64 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  v2f64 det0, det1, det2, tmp8, tmp9, tmp10, tmp11;
  const v2f64 rdet = COPY_DOUBLE_TO_VECTOR(rDet);
  // mat0 mat1 --> m00 m01 m02 m03
  // mat2 mat3 --> m10 m11 m12 m13
  // mat4 mat5 --> m20 m21 m22 m23
  // mat6 mat7 --> m30 m31 m32 m33
  LD_DP8(mat, 2, mat0, mat1, mat2, mat3, mat4, mat5, mat6, mat7);

  // Right half
  rev3 = SLDI_D(mat3, mat3, 8);  // m13 m12
  rev5 = SLDI_D(mat5, mat5, 8);  // m23 m22
  rev7 = SLDI_D(mat7, mat7, 8);  // m33 m32

  // 2*2 Determinants
  // for A00 & A01
  tmp0 = mat5 * rev7;
  tmp1 = mat3 * rev7;
  tmp2 = mat3 * rev5;
  // for A10 & A11
  tmp3 = mat1 * rev7;
  tmp4 = mat1 * rev5;
  // for A20 & A21
  tmp5 = mat1 * rev3;
  // for A30 & A31
  tmp6 = (v2f64)__msa_ilvr_d((v2i64)tmp1, (v2i64)tmp0);
  tmp7 = (v2f64)__msa_ilvl_d((v2i64)tmp1, (v2i64)tmp0);
  det0 = tmp6 - tmp7;
  tmp6 = (v2f64)__msa_ilvr_d((v2i64)tmp3, (v2i64)tmp2);
  tmp7 = (v2f64)__msa_ilvl_d((v2i64)tmp3, (v2i64)tmp2);
  det1 = tmp6 - tmp7;
  tmp6 = (v2f64)__msa_ilvr_d((v2i64)tmp5, (v2i64)tmp4);
  tmp7 = (v2f64)__msa_ilvl_d((v2i64)tmp5, (v2i64)tmp4);
  det2 = tmp6 - tmp7;

  // Co-factors
  tmp0 = mat0 * (v2f64)__msa_splati_d((v2i64)det0, 0);
  tmp1 = mat0 * (v2f64)__msa_splati_d((v2i64)det0, 1);
  tmp2 = mat0 * (v2f64)__msa_splati_d((v2i64)det1, 0);
  tmp3 = mat2 * (v2f64)__msa_splati_d((v2i64)det0, 0);
  tmp4 = mat2 * (v2f64)__msa_splati_d((v2i64)det1, 1);
  tmp5 = mat2 * (v2f64)__msa_splati_d((v2i64)det2, 0);
  tmp6 = mat4 * (v2f64)__msa_splati_d((v2i64)det0, 1);
  tmp7 = mat4 * (v2f64)__msa_splati_d((v2i64)det1, 1);
  tmp8 = mat4 * (v2f64)__msa_splati_d((v2i64)det2, 1);
  tmp9 = mat6 * (v2f64)__msa_splati_d((v2i64)det1, 0);
  tmp10 = mat6 * (v2f64)__msa_splati_d((v2i64)det2, 0);
  tmp11 = mat6 * (v2f64)__msa_splati_d((v2i64)det2, 1);

  tmp0 -= tmp7;
  tmp1 -= tmp4;
  tmp2 -= tmp5;
  tmp3 -= tmp6;
  tmp0 += tmp10;
  tmp1 += tmp11;
  tmp2 += tmp8;
  tmp3 += tmp9;

  // Multiply with 1/det
  tmp0 *= rdet;
  tmp1 *= rdet;
  tmp2 *= rdet;
  tmp3 *= rdet;

  // Inverse: Upper half
  result[0][0] = tmp3[1];
  result[0][1] = -tmp0[1];
  result[0][2] = tmp1[1];
  result[0][3] = -tmp2[1];
  result[1][0] = -tmp3[0];
  result[1][1] = tmp0[0];
  result[1][2] = -tmp1[0];
  result[1][3] = tmp2[0];
  // Left half
  rev2 = SLDI_D(mat2, mat2, 8);  // m13 m12
  rev4 = SLDI_D(mat4, mat4, 8);  // m23 m22
  rev6 = SLDI_D(mat6, mat6, 8);  // m33 m32

  // 2*2 Determinants
  // for A00 & A01
  tmp0 = mat4 * rev6;
  tmp1 = mat2 * rev6;
  tmp2 = mat2 * rev4;
  // for A10 & A11
  tmp3 = mat0 * rev6;
  tmp4 = mat0 * rev4;
  // for A20 & A21
  tmp5 = mat0 * rev2;
  // for A30 & A31
  tmp6 = (v2f64)__msa_ilvr_d((v2i64)tmp1, (v2i64)tmp0);
  tmp7 = (v2f64)__msa_ilvl_d((v2i64)tmp1, (v2i64)tmp0);
  det0 = tmp6 - tmp7;
  tmp6 = (v2f64)__msa_ilvr_d((v2i64)tmp3, (v2i64)tmp2);
  tmp7 = (v2f64)__msa_ilvl_d((v2i64)tmp3, (v2i64)tmp2);
  det1 = tmp6 - tmp7;
  tmp6 = (v2f64)__msa_ilvr_d((v2i64)tmp5, (v2i64)tmp4);
  tmp7 = (v2f64)__msa_ilvl_d((v2i64)tmp5, (v2i64)tmp4);
  det2 = tmp6 - tmp7;

  // Co-factors
  tmp0 = mat3 * (v2f64)__msa_splati_d((v2i64)det0, 0);
  tmp1 = mat1 * (v2f64)__msa_splati_d((v2i64)det0, 1);
  tmp2 = mat1 * (v2f64)__msa_splati_d((v2i64)det0, 0);
  tmp3 = mat1 * (v2f64)__msa_splati_d((v2i64)det1, 0);
  tmp4 = mat3 * (v2f64)__msa_splati_d((v2i64)det1, 1);
  tmp5 = mat3 * (v2f64)__msa_splati_d((v2i64)det2, 0);
  tmp6 = mat5 * (v2f64)__msa_splati_d((v2i64)det0, 1);
  tmp7 = mat5 * (v2f64)__msa_splati_d((v2i64)det1, 1);
  tmp8 = mat5 * (v2f64)__msa_splati_d((v2i64)det2, 1);
  tmp9 = mat7 * (v2f64)__msa_splati_d((v2i64)det1, 0);
  tmp10 = mat7 * (v2f64)__msa_splati_d((v2i64)det2, 0);
  tmp11 = mat7 * (v2f64)__msa_splati_d((v2i64)det2, 1);
  tmp0 -= tmp6;
  tmp1 -= tmp4;
  tmp2 -= tmp7;
  tmp3 -= tmp5;
  tmp0 += tmp9;
  tmp1 += tmp11;
  tmp2 += tmp10;
  tmp3 += tmp8;

  // Multiply with 1/det
  tmp0 *= rdet;
  tmp1 *= rdet;
  tmp2 *= rdet;
  tmp3 *= rdet;

  // Inverse: Lower half
  result[2][0] = tmp0[1];
  result[2][1] = -tmp2[1];
  result[2][2] = tmp1[1];
  result[2][3] = -tmp3[1];
  result[3][0] = -tmp0[0];
  result[3][1] = tmp2[0];
  result[3][2] = -tmp1[0];
  result[3][3] = tmp3[0];
#else
  // Calculate the adjoint matrix
  Adjoint(matrix, result);

  double rdet = 1 / det;

  // Scale the adjoint matrix to get the inverse
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      result[i][j] = result[i][j] * rdet;
#endif
  return true;
}

// End of code adapted from Matrix Inversion by Richard Carling

// Perform a decomposition on the passed matrix, return false if unsuccessful
// From Graphics Gems: unmatrix.c

// Transpose rotation portion of matrix a, return b
static void TransposeMatrix4(const TransformationMatrix::Matrix4& a,
                             TransformationMatrix::Matrix4& b) {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      b[i][j] = a[j][i];
}

// Multiply a homogeneous point by a matrix and return the transformed point
static void V4MulPointByMatrix(const Vector4 p,
                               const TransformationMatrix::Matrix4& m,
                               Vector4 result) {
  result[0] =
      (p[0] * m[0][0]) + (p[1] * m[1][0]) + (p[2] * m[2][0]) + (p[3] * m[3][0]);
  result[1] =
      (p[0] * m[0][1]) + (p[1] * m[1][1]) + (p[2] * m[2][1]) + (p[3] * m[3][1]);
  result[2] =
      (p[0] * m[0][2]) + (p[1] * m[1][2]) + (p[2] * m[2][2]) + (p[3] * m[3][2]);
  result[3] =
      (p[0] * m[0][3]) + (p[1] * m[1][3]) + (p[2] * m[2][3]) + (p[3] * m[3][3]);
}

static double V3Length(Vector3 a) {
  return std::sqrt((a[0] * a[0]) + (a[1] * a[1]) + (a[2] * a[2]));
}

static void V3Scale(Vector3 v, double desired_length) {
  double len = V3Length(v);
  if (len != 0) {
    double l = desired_length / len;
    v[0] *= l;
    v[1] *= l;
    v[2] *= l;
  }
}

static double V3Dot(const Vector3 a, const Vector3 b) {
  return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

// Make a linear combination of two vectors and return the result.
// result = (a * ascl) + (b * bscl)
static void V3Combine(const Vector3 a,
                      const Vector3 b,
                      Vector3 result,
                      double ascl,
                      double bscl) {
  result[0] = (ascl * a[0]) + (bscl * b[0]);
  result[1] = (ascl * a[1]) + (bscl * b[1]);
  result[2] = (ascl * a[2]) + (bscl * b[2]);
}

// Return the cross product result = a cross b */
static void V3Cross(const Vector3 a, const Vector3 b, Vector3 result) {
  result[0] = (a[1] * b[2]) - (a[2] * b[1]);
  result[1] = (a[2] * b[0]) - (a[0] * b[2]);
  result[2] = (a[0] * b[1]) - (a[1] * b[0]);
}

// TODO(crbug/937296): This implementation is virtually identical to the
// implementation in ui/gfx/transform_util with the main difference being
// the representation of the underlying matrix. These implementations should be
// consolidated.
static bool Decompose(const TransformationMatrix::Matrix4& mat,
                      TransformationMatrix::DecomposedType& result) {
  TransformationMatrix::Matrix4 local_matrix;
  memcpy(&local_matrix, &mat, sizeof(TransformationMatrix::Matrix4));

  // Normalize the matrix.
  if (local_matrix[3][3] == 0)
    return false;

  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      local_matrix[i][j] /= local_matrix[3][3];

  // perspectiveMatrix is used to solve for perspective, but it also provides
  // an easy way to test for singularity of the upper 3x3 component.
  TransformationMatrix::Matrix4 perspective_matrix;
  memcpy(&perspective_matrix, &local_matrix,
         sizeof(TransformationMatrix::Matrix4));
  for (i = 0; i < 3; i++)
    perspective_matrix[i][3] = 0;
  perspective_matrix[3][3] = 1;

  if (Determinant4x4(perspective_matrix) == 0)
    return false;

  // First, isolate perspective.  This is the messiest.
  if (local_matrix[0][3] != 0 || local_matrix[1][3] != 0 ||
      local_matrix[2][3] != 0) {
    // rightHandSide is the right hand side of the equation.
    Vector4 right_hand_side;
    right_hand_side[0] = local_matrix[0][3];
    right_hand_side[1] = local_matrix[1][3];
    right_hand_side[2] = local_matrix[2][3];
    right_hand_side[3] = local_matrix[3][3];

    // Solve the equation by inverting perspectiveMatrix and multiplying
    // rightHandSide by the inverse.  (This is the easiest way, not
    // necessarily the best.)
    TransformationMatrix::Matrix4 inverse_perspective_matrix,
        transposed_inverse_perspective_matrix;
    if (!Inverse(perspective_matrix, inverse_perspective_matrix))
      return false;
    TransposeMatrix4(inverse_perspective_matrix,
                     transposed_inverse_perspective_matrix);

    Vector4 perspective_point;
    V4MulPointByMatrix(right_hand_side, transposed_inverse_perspective_matrix,
                       perspective_point);

    result.perspective_x = perspective_point[0];
    result.perspective_y = perspective_point[1];
    result.perspective_z = perspective_point[2];
    result.perspective_w = perspective_point[3];

    // Clear the perspective partition
    local_matrix[0][3] = local_matrix[1][3] = local_matrix[2][3] = 0;
    local_matrix[3][3] = 1;
  } else {
    // No perspective.
    result.perspective_x = result.perspective_y = result.perspective_z = 0;
    result.perspective_w = 1;
  }

  // Next take care of translation (easy).
  result.translate_x = local_matrix[3][0];
  local_matrix[3][0] = 0;
  result.translate_y = local_matrix[3][1];
  local_matrix[3][1] = 0;
  result.translate_z = local_matrix[3][2];
  local_matrix[3][2] = 0;

  // Vector4 type and functions need to be added to the common set.
  Vector3 row[3], pdum3;

  // Now get scale and shear.
  for (i = 0; i < 3; i++) {
    row[i][0] = local_matrix[i][0];
    row[i][1] = local_matrix[i][1];
    row[i][2] = local_matrix[i][2];
  }

  // Compute X scale factor and normalize first row.
  result.scale_x = V3Length(row[0]);
  V3Scale(row[0], 1.0);

  // Compute XY shear factor and make 2nd row orthogonal to 1st.
  result.skew_xy = V3Dot(row[0], row[1]);
  V3Combine(row[1], row[0], row[1], 1.0, -result.skew_xy);

  // Now, compute Y scale and normalize 2nd row.
  result.scale_y = V3Length(row[1]);
  V3Scale(row[1], 1.0);
  result.skew_xy /= result.scale_y;

  // Compute XZ and YZ shears, orthogonalize 3rd row.
  result.skew_xz = V3Dot(row[0], row[2]);
  V3Combine(row[2], row[0], row[2], 1.0, -result.skew_xz);
  result.skew_yz = V3Dot(row[1], row[2]);
  V3Combine(row[2], row[1], row[2], 1.0, -result.skew_yz);

  // Next, get Z scale and normalize 3rd row.
  result.scale_z = V3Length(row[2]);
  V3Scale(row[2], 1.0);
  result.skew_xz /= result.scale_z;
  result.skew_yz /= result.scale_z;

  // At this point, the matrix (in rows[]) is orthonormal.
  // Check for a coordinate system flip.  If the determinant
  // is -1, then negate the matrix and the scaling factors.
  V3Cross(row[1], row[2], pdum3);
  if (V3Dot(row[0], pdum3) < 0) {
    result.scale_x *= -1;
    result.scale_y *= -1;
    result.scale_z *= -1;

    for (i = 0; i < 3; i++) {
      row[i][0] *= -1;
      row[i][1] *= -1;
      row[i][2] *= -1;
    }
  }

  // Now, get the rotations out, as described in the gem.

  // FIXME - Add the ability to return either quaternions (which are
  // easier to recompose with) or Euler angles (rx, ry, rz), which
  // are easier for authors to deal with. The latter will only be useful
  // when we fix https://bugs.webkit.org/show_bug.cgi?id=23799, so I
  // will leave the Euler angle code here for now.

  // ret.rotateY = asin(-row[0][2]);
  // if (cos(ret.rotateY) != 0) {
  //     ret.rotateX = atan2(row[1][2], row[2][2]);
  //     ret.rotateZ = atan2(row[0][1], row[0][0]);
  // } else {
  //     ret.rotateX = atan2(-row[2][0], row[1][1]);
  //     ret.rotateZ = 0;
  // }

  double s, t, x, y, z, w;

  t = row[0][0] + row[1][1] + row[2][2] + 1.0;

  if (t > 1e-4) {
    s = 0.5 / std::sqrt(t);
    w = 0.25 / s;
    x = (row[2][1] - row[1][2]) * s;
    y = (row[0][2] - row[2][0]) * s;
    z = (row[1][0] - row[0][1]) * s;
  } else if (row[0][0] > row[1][1] && row[0][0] > row[2][2]) {
    s = std::sqrt(1.0 + row[0][0] - row[1][1] - row[2][2]) * 2.0;  // S=4*qx
    x = 0.25 * s;
    y = (row[0][1] + row[1][0]) / s;
    z = (row[0][2] + row[2][0]) / s;
    w = (row[2][1] - row[1][2]) / s;
  } else if (row[1][1] > row[2][2]) {
    s = std::sqrt(1.0 + row[1][1] - row[0][0] - row[2][2]) * 2.0;  // S=4*qy
    x = (row[0][1] + row[1][0]) / s;
    y = 0.25 * s;
    z = (row[1][2] + row[2][1]) / s;
    w = (row[0][2] - row[2][0]) / s;
  } else {
    s = std::sqrt(1.0 + row[2][2] - row[0][0] - row[1][1]) * 2.0;  // S=4*qz
    x = (row[0][2] + row[2][0]) / s;
    y = (row[1][2] + row[2][1]) / s;
    z = 0.25 * s;
    w = (row[1][0] - row[0][1]) / s;
  }

  result.quaternion_x = x;
  result.quaternion_y = y;
  result.quaternion_z = z;
  result.quaternion_w = w;

  return true;
}

// Perform a spherical linear interpolation between the two
// passed quaternions with 0 <= t <= 1
static void Slerp(double qa[4], const double qb[4], double t) {
  double ax, ay, az, aw;
  double bx, by, bz, bw;
  double cx, cy, cz, cw;
  double product;

  ax = qa[0];
  ay = qa[1];
  az = qa[2];
  aw = qa[3];
  bx = qb[0];
  by = qb[1];
  bz = qb[2];
  bw = qb[3];

  product = ax * bx + ay * by + az * bz + aw * bw;

  product = clampTo(product, -1.0, 1.0);

  const double kEpsilon = 1e-5;
  if (std::abs(product - 1.0) < kEpsilon) {
    // Result is qa, so just return
    return;
  }

  double denom = std::sqrt(1.0 - product * product);
  double theta = std::acos(product);
  double w = std::sin(t * theta) * (1.0 / denom);

  double scale1 = std::cos(t * theta) - product * w;
  double scale2 = w;

  cx = ax * scale1 + bx * scale2;
  cy = ay * scale1 + by * scale2;
  cz = az * scale1 + bz * scale2;
  cw = aw * scale1 + bw * scale2;

  qa[0] = cx;
  qa[1] = cy;
  qa[2] = cz;
  qa[3] = cw;
}

// End of Supporting Math Functions

TransformationMatrix::TransformationMatrix(const AffineTransform& t) {
  SetMatrix(t.A(), t.B(), t.C(), t.D(), t.E(), t.F());
}

TransformationMatrix& TransformationMatrix::Scale(double s) {
  return ScaleNonUniform(s, s);
}

FloatPoint TransformationMatrix::ProjectPoint(const FloatPoint& p,
                                              bool* clamped) const {
  // This is basically raytracing. We have a point in the destination
  // plane with z=0, and we cast a ray parallel to the z-axis from that
  // point to find the z-position at which it intersects the z=0 plane
  // with the transform applied. Once we have that point we apply the
  // inverse transform to find the corresponding point in the source
  // space.
  //
  // Given a plane with normal Pn, and a ray starting at point R0 and
  // with direction defined by the vector Rd, we can find the
  // intersection point as a distance d from R0 in units of Rd by:
  //
  // d = -dot (Pn', R0) / dot (Pn', Rd)
  if (clamped)
    *clamped = false;

  if (M33() == 0) {
    // In this case, the projection plane is parallel to the ray we are trying
    // to trace, and there is no well-defined value for the projection.
    return FloatPoint();
  }

  double x = p.X();
  double y = p.Y();
  double z = -(M13() * x + M23() * y + M43()) / M33();

  // FIXME: use multVecMatrix()
  double out_x = x * M11() + y * M21() + z * M31() + M41();
  double out_y = x * M12() + y * M22() + z * M32() + M42();

  double w = x * M14() + y * M24() + z * M34() + M44();
  if (w <= 0) {
    // Using int max causes overflow when other code uses the projected point.
    // To represent infinity yet reduce the risk of overflow, we use a large but
    // not-too-large number here when clamping.
    const int kLargeNumber = 100000000 / kFixedPointDenominator;
    out_x = copysign(kLargeNumber, out_x);
    out_y = copysign(kLargeNumber, out_y);
    if (clamped)
      *clamped = true;
  } else if (w != 1) {
    out_x /= w;
    out_y /= w;
  }

  return FloatPoint(static_cast<float>(out_x), static_cast<float>(out_y));
}

FloatQuad TransformationMatrix::ProjectQuad(const FloatQuad& q,
                                            bool* clamped) const {
  FloatQuad projected_quad;

  bool clamped1 = false;
  bool clamped2 = false;
  bool clamped3 = false;
  bool clamped4 = false;

  projected_quad.SetP1(ProjectPoint(q.P1(), &clamped1));
  projected_quad.SetP2(ProjectPoint(q.P2(), &clamped2));
  projected_quad.SetP3(ProjectPoint(q.P3(), &clamped3));
  projected_quad.SetP4(ProjectPoint(q.P4(), &clamped4));

  if (clamped)
    *clamped = clamped1 || clamped2 || clamped3 || clamped4;

  // If all points on the quad had w < 0, then the entire quad would not be
  // visible to the projected surface.
  bool everything_was_clipped = clamped1 && clamped2 && clamped3 && clamped4;
  if (everything_was_clipped)
    return FloatQuad();

  return projected_quad;
}

static float ClampEdgeValue(float f) {
  DCHECK(!std::isnan(f));
  return clampTo(f, (-LayoutUnit::Max() / 2).ToFloat(),
                 (LayoutUnit::Max() / 2).ToFloat());
}

LayoutRect TransformationMatrix::ClampedBoundsOfProjectedQuad(
    const FloatQuad& q) const {
  FloatRect mapped_quad_bounds = ProjectQuad(q).BoundingBox();

  float left = ClampEdgeValue(floorf(mapped_quad_bounds.X()));
  float top = ClampEdgeValue(floorf(mapped_quad_bounds.Y()));

  float right;
  if (std::isinf(mapped_quad_bounds.X()) &&
      std::isinf(mapped_quad_bounds.Width()))
    right = (LayoutUnit::Max() / 2).ToFloat();
  else
    right = ClampEdgeValue(ceilf(mapped_quad_bounds.MaxX()));

  float bottom;
  if (std::isinf(mapped_quad_bounds.Y()) &&
      std::isinf(mapped_quad_bounds.Height()))
    bottom = (LayoutUnit::Max() / 2).ToFloat();
  else
    bottom = ClampEdgeValue(ceilf(mapped_quad_bounds.MaxY()));

  return LayoutRect(LayoutUnit::Clamp(left), LayoutUnit::Clamp(top),
                    LayoutUnit::Clamp(right - left),
                    LayoutUnit::Clamp(bottom - top));
}

void TransformationMatrix::TransformBox(FloatBox& box) const {
  FloatBox bounds;
  bool first_point = true;
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      for (size_t k = 0; k < 2; ++k) {
        FloatPoint3D point(box.X(), box.Y(), box.Z());
        point +=
            FloatPoint3D(i * box.Width(), j * box.Height(), k * box.Depth());
        point = MapPoint(point);
        if (first_point) {
          bounds.SetOrigin(point);
          first_point = false;
        } else {
          bounds.ExpandTo(point);
        }
      }
    }
  }
  box = bounds;
}

FloatPoint TransformationMatrix::MapPoint(const FloatPoint& p) const {
  if (IsIdentityOrTranslation())
    return FloatPoint(p.X() + static_cast<float>(matrix_[3][0]),
                      p.Y() + static_cast<float>(matrix_[3][1]));

  return InternalMapPoint(p);
}

FloatPoint3D TransformationMatrix::MapPoint(const FloatPoint3D& p) const {
  if (IsIdentityOrTranslation())
    return FloatPoint3D(p.X() + static_cast<float>(matrix_[3][0]),
                        p.Y() + static_cast<float>(matrix_[3][1]),
                        p.Z() + static_cast<float>(matrix_[3][2]));

  return InternalMapPoint(p);
}

IntRect TransformationMatrix::MapRect(const IntRect& rect) const {
  return EnclosingIntRect(MapRect(FloatRect(rect)));
}

LayoutRect TransformationMatrix::MapRect(const LayoutRect& r) const {
  return EnclosingLayoutRect(MapRect(FloatRect(r)));
}

FloatRect TransformationMatrix::MapRect(const FloatRect& r) const {
  if (IsIdentityOrTranslation()) {
    FloatRect mapped_rect(r);
    mapped_rect.Move(static_cast<float>(matrix_[3][0]),
                     static_cast<float>(matrix_[3][1]));
    return mapped_rect;
  }

  FloatQuad result;

  float max_x = r.MaxX();
  float max_y = r.MaxY();
  result.SetP1(InternalMapPoint(FloatPoint(r.X(), r.Y())));
  result.SetP2(InternalMapPoint(FloatPoint(max_x, r.Y())));
  result.SetP3(InternalMapPoint(FloatPoint(max_x, max_y)));
  result.SetP4(InternalMapPoint(FloatPoint(r.X(), max_y)));

  return result.BoundingBox();
}

FloatQuad TransformationMatrix::MapQuad(const FloatQuad& q) const {
  if (IsIdentityOrTranslation()) {
    FloatQuad mapped_quad(q);
    mapped_quad.Move(static_cast<float>(matrix_[3][0]),
                     static_cast<float>(matrix_[3][1]));
    return mapped_quad;
  }

  FloatQuad result;
  result.SetP1(InternalMapPoint(q.P1()));
  result.SetP2(InternalMapPoint(q.P2()));
  result.SetP3(InternalMapPoint(q.P3()));
  result.SetP4(InternalMapPoint(q.P4()));
  return result;
}

TransformationMatrix& TransformationMatrix::ScaleNonUniform(double sx,
                                                            double sy) {
  matrix_[0][0] *= sx;
  matrix_[0][1] *= sx;
  matrix_[0][2] *= sx;
  matrix_[0][3] *= sx;

  matrix_[1][0] *= sy;
  matrix_[1][1] *= sy;
  matrix_[1][2] *= sy;
  matrix_[1][3] *= sy;
  return *this;
}

TransformationMatrix& TransformationMatrix::Scale3d(double sx,
                                                    double sy,
                                                    double sz) {
  ScaleNonUniform(sx, sy);

  matrix_[2][0] *= sz;
  matrix_[2][1] *= sz;
  matrix_[2][2] *= sz;
  matrix_[2][3] *= sz;
  return *this;
}

TransformationMatrix& TransformationMatrix::Rotate3d(const Rotation& rotation) {
  return Rotate3d(rotation.axis.X(), rotation.axis.Y(), rotation.axis.Z(),
                  rotation.angle);
}

TransformationMatrix& TransformationMatrix::Rotate3d(double x,
                                                     double y,
                                                     double z,
                                                     double angle) {
  // Normalize the axis of rotation
  double length = std::sqrt(x * x + y * y + z * z);
  if (length == 0) {
    // A direction vector that cannot be normalized, such as [0, 0, 0], will
    // cause the rotation to not be applied.
    return *this;
  } else if (length != 1) {
    x /= length;
    y /= length;
    z /= length;
  }

  // Angles are in degrees. Switch to radians.
  angle = deg2rad(angle);

  double sin_theta = std::sin(angle);
  double cos_theta = std::cos(angle);

  TransformationMatrix mat;

  // Optimize cases where the axis is along a major axis
  if (x == 1.0 && y == 0.0 && z == 0.0) {
    mat.matrix_[0][0] = 1.0;
    mat.matrix_[0][1] = 0.0;
    mat.matrix_[0][2] = 0.0;
    mat.matrix_[1][0] = 0.0;
    mat.matrix_[1][1] = cos_theta;
    mat.matrix_[1][2] = sin_theta;
    mat.matrix_[2][0] = 0.0;
    mat.matrix_[2][1] = -sin_theta;
    mat.matrix_[2][2] = cos_theta;
    mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
    mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
    mat.matrix_[3][3] = 1.0;
  } else if (x == 0.0 && y == 1.0 && z == 0.0) {
    mat.matrix_[0][0] = cos_theta;
    mat.matrix_[0][1] = 0.0;
    mat.matrix_[0][2] = -sin_theta;
    mat.matrix_[1][0] = 0.0;
    mat.matrix_[1][1] = 1.0;
    mat.matrix_[1][2] = 0.0;
    mat.matrix_[2][0] = sin_theta;
    mat.matrix_[2][1] = 0.0;
    mat.matrix_[2][2] = cos_theta;
    mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
    mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
    mat.matrix_[3][3] = 1.0;
  } else if (x == 0.0 && y == 0.0 && z == 1.0) {
    mat.matrix_[0][0] = cos_theta;
    mat.matrix_[0][1] = sin_theta;
    mat.matrix_[0][2] = 0.0;
    mat.matrix_[1][0] = -sin_theta;
    mat.matrix_[1][1] = cos_theta;
    mat.matrix_[1][2] = 0.0;
    mat.matrix_[2][0] = 0.0;
    mat.matrix_[2][1] = 0.0;
    mat.matrix_[2][2] = 1.0;
    mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
    mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
    mat.matrix_[3][3] = 1.0;
  } else {
    // This case is the rotation about an arbitrary unit vector.
    //
    // Formula is adapted from Wikipedia article on Rotation matrix,
    // http://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
    //
    // An alternate resource with the same matrix:
    // http://www.fastgraph.com/makegames/3drotation/
    //
    double one_minus_cos_theta = 1 - cos_theta;
    mat.matrix_[0][0] = cos_theta + x * x * one_minus_cos_theta;
    mat.matrix_[0][1] = y * x * one_minus_cos_theta + z * sin_theta;
    mat.matrix_[0][2] = z * x * one_minus_cos_theta - y * sin_theta;
    mat.matrix_[1][0] = x * y * one_minus_cos_theta - z * sin_theta;
    mat.matrix_[1][1] = cos_theta + y * y * one_minus_cos_theta;
    mat.matrix_[1][2] = z * y * one_minus_cos_theta + x * sin_theta;
    mat.matrix_[2][0] = x * z * one_minus_cos_theta + y * sin_theta;
    mat.matrix_[2][1] = y * z * one_minus_cos_theta - x * sin_theta;
    mat.matrix_[2][2] = cos_theta + z * z * one_minus_cos_theta;
    mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
    mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
    mat.matrix_[3][3] = 1.0;
  }
  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::Rotate3d(double rx,
                                                     double ry,
                                                     double rz) {
  // Angles are in degrees. Switch to radians.
  rx = deg2rad(rx);
  ry = deg2rad(ry);
  rz = deg2rad(rz);

  TransformationMatrix mat;

  double sin_theta = std::sin(rz);
  double cos_theta = std::cos(rz);

  mat.matrix_[0][0] = cos_theta;
  mat.matrix_[0][1] = sin_theta;
  mat.matrix_[0][2] = 0.0;
  mat.matrix_[1][0] = -sin_theta;
  mat.matrix_[1][1] = cos_theta;
  mat.matrix_[1][2] = 0.0;
  mat.matrix_[2][0] = 0.0;
  mat.matrix_[2][1] = 0.0;
  mat.matrix_[2][2] = 1.0;
  mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
  mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
  mat.matrix_[3][3] = 1.0;

  TransformationMatrix rmat(mat);

  sin_theta = std::sin(ry);
  cos_theta = std::cos(ry);

  mat.matrix_[0][0] = cos_theta;
  mat.matrix_[0][1] = 0.0;
  mat.matrix_[0][2] = -sin_theta;
  mat.matrix_[1][0] = 0.0;
  mat.matrix_[1][1] = 1.0;
  mat.matrix_[1][2] = 0.0;
  mat.matrix_[2][0] = sin_theta;
  mat.matrix_[2][1] = 0.0;
  mat.matrix_[2][2] = cos_theta;
  mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
  mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
  mat.matrix_[3][3] = 1.0;

  rmat.Multiply(mat);

  sin_theta = std::sin(rx);
  cos_theta = std::cos(rx);

  mat.matrix_[0][0] = 1.0;
  mat.matrix_[0][1] = 0.0;
  mat.matrix_[0][2] = 0.0;
  mat.matrix_[1][0] = 0.0;
  mat.matrix_[1][1] = cos_theta;
  mat.matrix_[1][2] = sin_theta;
  mat.matrix_[2][0] = 0.0;
  mat.matrix_[2][1] = -sin_theta;
  mat.matrix_[2][2] = cos_theta;
  mat.matrix_[0][3] = mat.matrix_[1][3] = mat.matrix_[2][3] = 0.0;
  mat.matrix_[3][0] = mat.matrix_[3][1] = mat.matrix_[3][2] = 0.0;
  mat.matrix_[3][3] = 1.0;

  rmat.Multiply(mat);

  Multiply(rmat);
  return *this;
}

TransformationMatrix& TransformationMatrix::Translate(double tx, double ty) {
  matrix_[3][0] += tx * matrix_[0][0] + ty * matrix_[1][0];
  matrix_[3][1] += tx * matrix_[0][1] + ty * matrix_[1][1];
  matrix_[3][2] += tx * matrix_[0][2] + ty * matrix_[1][2];
  matrix_[3][3] += tx * matrix_[0][3] + ty * matrix_[1][3];
  return *this;
}

TransformationMatrix& TransformationMatrix::Translate3d(double tx,
                                                        double ty,
                                                        double tz) {
  matrix_[3][0] += tx * matrix_[0][0] + ty * matrix_[1][0] + tz * matrix_[2][0];
  matrix_[3][1] += tx * matrix_[0][1] + ty * matrix_[1][1] + tz * matrix_[2][1];
  matrix_[3][2] += tx * matrix_[0][2] + ty * matrix_[1][2] + tz * matrix_[2][2];
  matrix_[3][3] += tx * matrix_[0][3] + ty * matrix_[1][3] + tz * matrix_[2][3];
  return *this;
}

TransformationMatrix& TransformationMatrix::PostTranslate(double tx,
                                                          double ty) {
  if (tx != 0) {
    matrix_[0][0] += matrix_[0][3] * tx;
    matrix_[1][0] += matrix_[1][3] * tx;
    matrix_[2][0] += matrix_[2][3] * tx;
    matrix_[3][0] += matrix_[3][3] * tx;
  }

  if (ty != 0) {
    matrix_[0][1] += matrix_[0][3] * ty;
    matrix_[1][1] += matrix_[1][3] * ty;
    matrix_[2][1] += matrix_[2][3] * ty;
    matrix_[3][1] += matrix_[3][3] * ty;
  }

  return *this;
}

TransformationMatrix& TransformationMatrix::PostTranslate3d(double tx,
                                                            double ty,
                                                            double tz) {
  PostTranslate(tx, ty);
  if (tz != 0) {
    matrix_[0][2] += matrix_[0][3] * tz;
    matrix_[1][2] += matrix_[1][3] * tz;
    matrix_[2][2] += matrix_[2][3] * tz;
    matrix_[3][2] += matrix_[3][3] * tz;
  }

  return *this;
}

TransformationMatrix& TransformationMatrix::Skew(double sx, double sy) {
  // angles are in degrees. Switch to radians
  sx = deg2rad(sx);
  sy = deg2rad(sy);

  TransformationMatrix mat;
  mat.matrix_[0][1] =
      std::tan(sy);  // note that the y shear goes in the first row
  mat.matrix_[1][0] = std::tan(sx);  // and the x shear in the second row

  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::ApplyPerspective(double p) {
  TransformationMatrix mat;
  if (p != 0)
    mat.matrix_[2][3] = -1 / p;

  Multiply(mat);
  return *this;
}

TransformationMatrix& TransformationMatrix::ApplyTransformOrigin(double x,
                                                                 double y,
                                                                 double z) {
  PostTranslate3d(x, y, z);
  Translate3d(-x, -y, -z);
  return *this;
}

TransformationMatrix& TransformationMatrix::Zoom(double zoom_factor) {
  matrix_[0][3] /= zoom_factor;
  matrix_[1][3] /= zoom_factor;
  matrix_[2][3] /= zoom_factor;
  matrix_[3][0] *= zoom_factor;
  matrix_[3][1] *= zoom_factor;
  matrix_[3][2] *= zoom_factor;
  return *this;
}

// Calculates *this = *this * mat.
// Note: As we are using the column vector convention, i.e. T * P,
// (lhs * rhs) * P = lhs * (rhs * P)
// That means from the perspective of the transformed object, the combined
// transform is equal to applying the rhs(mat) first, then lhs(*this) second.
// For example:
// TransformationMatrix lhs; lhs.Rotate(90.f);
// TransformationMatrix rhs; rhs.Translate(12.f, 34.f);
// TransformationMatrix prod = lhs;
// prod.Multiply(rhs);
// lhs.MapPoint(rhs.MapPoint(p)) == prod.MapPoint(p)
// Also 'prod' corresponds to CSS transform:rotateZ(90deg)translate(12px,34px).
TransformationMatrix& TransformationMatrix::Multiply(
    const TransformationMatrix& mat) {
#if defined(ARCH_CPU_ARM64)
  double* left_matrix = &(matrix_[0][0]);
  const double* right_matrix = &(mat.matrix_[0][0]);
  asm volatile(
      // Load this->matrix_ to v24 - v31.
      // Load mat.matrix_ to v16 - v23.
      // Result: *this = *this * mat
      // | v0 v2 v4 v6 |   | v24 v26 v28 v30 |   | v16 v18 v20 v22 |
      // |             | = |                 | * |                 |
      // | v1 v3 v5 v7 |   | v25 v27 v29 v31 |   | v17 v19 v21 v23 |
      // |             |   |                 |   |                 |
      "mov x9, %[left_matrix]   \t\n"
      "ld1 {v16.2d - v19.2d}, [%[right_matrix]], 64  \t\n"
      "ld1 {v20.2d - v23.2d}, [%[right_matrix]]      \t\n"
      "ld1 {v24.2d - v27.2d}, [%[left_matrix]], 64 \t\n"
      "ld1 {v28.2d - v31.2d}, [%[left_matrix]]     \t\n"

      "fmul v0.2d, v24.2d, v16.d[0]  \t\n"
      "fmul v1.2d, v25.2d, v16.d[0]  \t\n"
      "fmul v2.2d, v24.2d, v18.d[0]  \t\n"
      "fmul v3.2d, v25.2d, v18.d[0]  \t\n"
      "fmul v4.2d, v24.2d, v20.d[0]  \t\n"
      "fmul v5.2d, v25.2d, v20.d[0]  \t\n"
      "fmul v6.2d, v24.2d, v22.d[0]  \t\n"
      "fmul v7.2d, v25.2d, v22.d[0]  \t\n"

      "fmla v0.2d, v26.2d, v16.d[1]  \t\n"
      "fmla v1.2d, v27.2d, v16.d[1]  \t\n"
      "fmla v2.2d, v26.2d, v18.d[1]  \t\n"
      "fmla v3.2d, v27.2d, v18.d[1]  \t\n"
      "fmla v4.2d, v26.2d, v20.d[1]  \t\n"
      "fmla v5.2d, v27.2d, v20.d[1]  \t\n"
      "fmla v6.2d, v26.2d, v22.d[1]  \t\n"
      "fmla v7.2d, v27.2d, v22.d[1]  \t\n"

      "fmla v0.2d, v28.2d, v17.d[0]  \t\n"
      "fmla v1.2d, v29.2d, v17.d[0]  \t\n"
      "fmla v2.2d, v28.2d, v19.d[0]  \t\n"
      "fmla v3.2d, v29.2d, v19.d[0]  \t\n"
      "fmla v4.2d, v28.2d, v21.d[0]  \t\n"
      "fmla v5.2d, v29.2d, v21.d[0]  \t\n"
      "fmla v6.2d, v28.2d, v23.d[0]  \t\n"
      "fmla v7.2d, v29.2d, v23.d[0]  \t\n"

      "fmla v0.2d, v30.2d, v17.d[1]  \t\n"
      "fmla v1.2d, v31.2d, v17.d[1]  \t\n"
      "fmla v2.2d, v30.2d, v19.d[1]  \t\n"
      "fmla v3.2d, v31.2d, v19.d[1]  \t\n"
      "fmla v4.2d, v30.2d, v21.d[1]  \t\n"
      "fmla v5.2d, v31.2d, v21.d[1]  \t\n"
      "fmla v6.2d, v30.2d, v23.d[1]  \t\n"
      "fmla v7.2d, v31.2d, v23.d[1]  \t\n"

      "st1 {v0.2d - v3.2d}, [x9], 64 \t\n"
      "st1 {v4.2d - v7.2d}, [x9]     \t\n"
      : [right_matrix] "+r"(right_matrix), [left_matrix] "+r"(left_matrix)
      :
      : "memory", "x9", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
        "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31", "v0", "v1",
        "v2", "v3", "v4", "v5", "v6", "v7");
#elif defined(HAVE_MIPS_MSA_INTRINSICS)
  v2f64 v_right_m0, v_right_m1, v_right_m2, v_right_m3, v_right_m4, v_right_m5,
      v_right_m6, v_right_m7;
  v2f64 v_left_m0, v_left_m1, v_left_m2, v_left_m3, v_left_m4, v_left_m5,
      v_left_m6, v_left_m7;
  v2f64 v_tmp_m0, v_tmp_m1, v_tmp_m2, v_tmp_m3;

  v_left_m0 = LD_DP(&(matrix_[0][0]));
  v_left_m1 = LD_DP(&(matrix_[0][2]));
  v_left_m2 = LD_DP(&(matrix_[1][0]));
  v_left_m3 = LD_DP(&(matrix_[1][2]));
  v_left_m4 = LD_DP(&(matrix_[2][0]));
  v_left_m5 = LD_DP(&(matrix_[2][2]));
  v_left_m6 = LD_DP(&(matrix_[3][0]));
  v_left_m7 = LD_DP(&(matrix_[3][2]));

  v_right_m0 = LD_DP(&(mat.matrix_[0][0]));
  v_right_m2 = LD_DP(&(mat.matrix_[0][2]));
  v_right_m4 = LD_DP(&(mat.matrix_[1][0]));
  v_right_m6 = LD_DP(&(mat.matrix_[1][2]));

  v_right_m1 = (v2f64)__msa_splati_d((v2i64)v_right_m0, 1);
  v_right_m0 = (v2f64)__msa_splati_d((v2i64)v_right_m0, 0);
  v_right_m3 = (v2f64)__msa_splati_d((v2i64)v_right_m2, 1);
  v_right_m2 = (v2f64)__msa_splati_d((v2i64)v_right_m2, 0);
  v_right_m5 = (v2f64)__msa_splati_d((v2i64)v_right_m4, 1);
  v_right_m4 = (v2f64)__msa_splati_d((v2i64)v_right_m4, 0);
  v_right_m7 = (v2f64)__msa_splati_d((v2i64)v_right_m6, 1);
  v_right_m6 = (v2f64)__msa_splati_d((v2i64)v_right_m6, 0);

  v_tmp_m0 = v_right_m0 * v_left_m0;
  v_tmp_m1 = v_right_m0 * v_left_m1;
  v_tmp_m0 += v_right_m1 * v_left_m2;
  v_tmp_m1 += v_right_m1 * v_left_m3;
  v_tmp_m0 += v_right_m2 * v_left_m4;
  v_tmp_m1 += v_right_m2 * v_left_m5;
  v_tmp_m0 += v_right_m3 * v_left_m6;
  v_tmp_m1 += v_right_m3 * v_left_m7;

  v_tmp_m2 = v_right_m4 * v_left_m0;
  v_tmp_m3 = v_right_m4 * v_left_m1;
  v_tmp_m2 += v_right_m5 * v_left_m2;
  v_tmp_m3 += v_right_m5 * v_left_m3;
  v_tmp_m2 += v_right_m6 * v_left_m4;
  v_tmp_m3 += v_right_m6 * v_left_m5;
  v_tmp_m2 += v_right_m7 * v_left_m6;
  v_tmp_m3 += v_right_m7 * v_left_m7;

  v_right_m0 = LD_DP(&(mat.matrix_[2][0]));
  v_right_m2 = LD_DP(&(mat.matrix_[2][2]));
  v_right_m4 = LD_DP(&(mat.matrix_[3][0]));
  v_right_m6 = LD_DP(&(mat.matrix_[3][2]));

  ST_DP(v_tmp_m0, &(matrix_[0][0]));
  ST_DP(v_tmp_m1, &(matrix_[0][2]));
  ST_DP(v_tmp_m2, &(matrix_[1][0]));
  ST_DP(v_tmp_m3, &(matrix_[1][2]));

  v_right_m1 = (v2f64)__msa_splati_d((v2i64)v_right_m0, 1);
  v_right_m0 = (v2f64)__msa_splati_d((v2i64)v_right_m0, 0);
  v_right_m3 = (v2f64)__msa_splati_d((v2i64)v_right_m2, 1);
  v_right_m2 = (v2f64)__msa_splati_d((v2i64)v_right_m2, 0);
  v_right_m5 = (v2f64)__msa_splati_d((v2i64)v_right_m4, 1);
  v_right_m4 = (v2f64)__msa_splati_d((v2i64)v_right_m4, 0);
  v_right_m7 = (v2f64)__msa_splati_d((v2i64)v_right_m6, 1);
  v_right_m6 = (v2f64)__msa_splati_d((v2i64)v_right_m6, 0);

  v_tmp_m0 = v_right_m0 * v_left_m0;
  v_tmp_m1 = v_right_m0 * v_left_m1;
  v_tmp_m0 += v_right_m1 * v_left_m2;
  v_tmp_m1 += v_right_m1 * v_left_m3;
  v_tmp_m0 += v_right_m2 * v_left_m4;
  v_tmp_m1 += v_right_m2 * v_left_m5;
  v_tmp_m0 += v_right_m3 * v_left_m6;
  v_tmp_m1 += v_right_m3 * v_left_m7;

  v_tmp_m2 = v_right_m4 * v_left_m0;
  v_tmp_m3 = v_right_m4 * v_left_m1;
  v_tmp_m2 += v_right_m5 * v_left_m2;
  v_tmp_m3 += v_right_m5 * v_left_m3;
  v_tmp_m2 += v_right_m6 * v_left_m4;
  v_tmp_m3 += v_right_m6 * v_left_m5;
  v_tmp_m2 += v_right_m7 * v_left_m6;
  v_tmp_m3 += v_right_m7 * v_left_m7;

  ST_DP(v_tmp_m0, &(matrix_[2][0]));
  ST_DP(v_tmp_m1, &(matrix_[2][2]));
  ST_DP(v_tmp_m2, &(matrix_[3][0]));
  ST_DP(v_tmp_m3, &(matrix_[3][2]));
#elif defined(ARCH_CPU_X86_64)
  // x86_64 has 16 XMM registers which is enough to do the multiplication fully
  // in registers.
  __m128d matrix_block_a = _mm_loadu_pd(&(matrix_[0][0]));
  __m128d matrix_block_c = _mm_loadu_pd(&(matrix_[1][0]));
  __m128d matrix_block_e = _mm_loadu_pd(&(matrix_[2][0]));
  __m128d matrix_block_g = _mm_loadu_pd(&(matrix_[3][0]));

  // First column.
  __m128d other_matrix_first_param = _mm_set1_pd(mat.matrix_[0][0]);
  __m128d other_matrix_second_param = _mm_set1_pd(mat.matrix_[0][1]);
  __m128d other_matrix_third_param = _mm_set1_pd(mat.matrix_[0][2]);
  __m128d other_matrix_fourth_param = _mm_set1_pd(mat.matrix_[0][3]);

  // output00 and output01.
  __m128d accumulator = _mm_mul_pd(matrix_block_a, other_matrix_first_param);
  __m128d temp1 = _mm_mul_pd(matrix_block_c, other_matrix_second_param);
  __m128d temp2 = _mm_mul_pd(matrix_block_e, other_matrix_third_param);
  __m128d temp3 = _mm_mul_pd(matrix_block_g, other_matrix_fourth_param);

  __m128d matrix_block_b = _mm_loadu_pd(&(matrix_[0][2]));
  __m128d matrix_block_d = _mm_loadu_pd(&(matrix_[1][2]));
  __m128d matrix_block_f = _mm_loadu_pd(&(matrix_[2][2]));
  __m128d matrix_block_h = _mm_loadu_pd(&(matrix_[3][2]));

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[0][0], accumulator);

  // output02 and output03.
  accumulator = _mm_mul_pd(matrix_block_b, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_d, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_f, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_h, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[0][2], accumulator);

  // Second column.
  other_matrix_first_param = _mm_set1_pd(mat.matrix_[1][0]);
  other_matrix_second_param = _mm_set1_pd(mat.matrix_[1][1]);
  other_matrix_third_param = _mm_set1_pd(mat.matrix_[1][2]);
  other_matrix_fourth_param = _mm_set1_pd(mat.matrix_[1][3]);

  // output10 and output11.
  accumulator = _mm_mul_pd(matrix_block_a, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_c, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_e, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_g, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[1][0], accumulator);

  // output12 and output13.
  accumulator = _mm_mul_pd(matrix_block_b, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_d, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_f, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_h, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[1][2], accumulator);

  // Third column.
  other_matrix_first_param = _mm_set1_pd(mat.matrix_[2][0]);
  other_matrix_second_param = _mm_set1_pd(mat.matrix_[2][1]);
  other_matrix_third_param = _mm_set1_pd(mat.matrix_[2][2]);
  other_matrix_fourth_param = _mm_set1_pd(mat.matrix_[2][3]);

  // output20 and output21.
  accumulator = _mm_mul_pd(matrix_block_a, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_c, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_e, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_g, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[2][0], accumulator);

  // output22 and output23.
  accumulator = _mm_mul_pd(matrix_block_b, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_d, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_f, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_h, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[2][2], accumulator);

  // Fourth column.
  other_matrix_first_param = _mm_set1_pd(mat.matrix_[3][0]);
  other_matrix_second_param = _mm_set1_pd(mat.matrix_[3][1]);
  other_matrix_third_param = _mm_set1_pd(mat.matrix_[3][2]);
  other_matrix_fourth_param = _mm_set1_pd(mat.matrix_[3][3]);

  // output30 and output31.
  accumulator = _mm_mul_pd(matrix_block_a, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_c, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_e, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_g, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[3][0], accumulator);

  // output32 and output33.
  accumulator = _mm_mul_pd(matrix_block_b, other_matrix_first_param);
  temp1 = _mm_mul_pd(matrix_block_d, other_matrix_second_param);
  temp2 = _mm_mul_pd(matrix_block_f, other_matrix_third_param);
  temp3 = _mm_mul_pd(matrix_block_h, other_matrix_fourth_param);

  accumulator = _mm_add_pd(accumulator, temp1);
  accumulator = _mm_add_pd(accumulator, temp2);
  accumulator = _mm_add_pd(accumulator, temp3);
  _mm_storeu_pd(&matrix_[3][2], accumulator);
#else
  Matrix4 tmp;

  tmp[0][0] =
      (mat.matrix_[0][0] * matrix_[0][0] + mat.matrix_[0][1] * matrix_[1][0] +
       mat.matrix_[0][2] * matrix_[2][0] + mat.matrix_[0][3] * matrix_[3][0]);
  tmp[0][1] =
      (mat.matrix_[0][0] * matrix_[0][1] + mat.matrix_[0][1] * matrix_[1][1] +
       mat.matrix_[0][2] * matrix_[2][1] + mat.matrix_[0][3] * matrix_[3][1]);
  tmp[0][2] =
      (mat.matrix_[0][0] * matrix_[0][2] + mat.matrix_[0][1] * matrix_[1][2] +
       mat.matrix_[0][2] * matrix_[2][2] + mat.matrix_[0][3] * matrix_[3][2]);
  tmp[0][3] =
      (mat.matrix_[0][0] * matrix_[0][3] + mat.matrix_[0][1] * matrix_[1][3] +
       mat.matrix_[0][2] * matrix_[2][3] + mat.matrix_[0][3] * matrix_[3][3]);

  tmp[1][0] =
      (mat.matrix_[1][0] * matrix_[0][0] + mat.matrix_[1][1] * matrix_[1][0] +
       mat.matrix_[1][2] * matrix_[2][0] + mat.matrix_[1][3] * matrix_[3][0]);
  tmp[1][1] =
      (mat.matrix_[1][0] * matrix_[0][1] + mat.matrix_[1][1] * matrix_[1][1] +
       mat.matrix_[1][2] * matrix_[2][1] + mat.matrix_[1][3] * matrix_[3][1]);
  tmp[1][2] =
      (mat.matrix_[1][0] * matrix_[0][2] + mat.matrix_[1][1] * matrix_[1][2] +
       mat.matrix_[1][2] * matrix_[2][2] + mat.matrix_[1][3] * matrix_[3][2]);
  tmp[1][3] =
      (mat.matrix_[1][0] * matrix_[0][3] + mat.matrix_[1][1] * matrix_[1][3] +
       mat.matrix_[1][2] * matrix_[2][3] + mat.matrix_[1][3] * matrix_[3][3]);

  tmp[2][0] =
      (mat.matrix_[2][0] * matrix_[0][0] + mat.matrix_[2][1] * matrix_[1][0] +
       mat.matrix_[2][2] * matrix_[2][0] + mat.matrix_[2][3] * matrix_[3][0]);
  tmp[2][1] =
      (mat.matrix_[2][0] * matrix_[0][1] + mat.matrix_[2][1] * matrix_[1][1] +
       mat.matrix_[2][2] * matrix_[2][1] + mat.matrix_[2][3] * matrix_[3][1]);
  tmp[2][2] =
      (mat.matrix_[2][0] * matrix_[0][2] + mat.matrix_[2][1] * matrix_[1][2] +
       mat.matrix_[2][2] * matrix_[2][2] + mat.matrix_[2][3] * matrix_[3][2]);
  tmp[2][3] =
      (mat.matrix_[2][0] * matrix_[0][3] + mat.matrix_[2][1] * matrix_[1][3] +
       mat.matrix_[2][2] * matrix_[2][3] + mat.matrix_[2][3] * matrix_[3][3]);

  tmp[3][0] =
      (mat.matrix_[3][0] * matrix_[0][0] + mat.matrix_[3][1] * matrix_[1][0] +
       mat.matrix_[3][2] * matrix_[2][0] + mat.matrix_[3][3] * matrix_[3][0]);
  tmp[3][1] =
      (mat.matrix_[3][0] * matrix_[0][1] + mat.matrix_[3][1] * matrix_[1][1] +
       mat.matrix_[3][2] * matrix_[2][1] + mat.matrix_[3][3] * matrix_[3][1]);
  tmp[3][2] =
      (mat.matrix_[3][0] * matrix_[0][2] + mat.matrix_[3][1] * matrix_[1][2] +
       mat.matrix_[3][2] * matrix_[2][2] + mat.matrix_[3][3] * matrix_[3][2]);
  tmp[3][3] =
      (mat.matrix_[3][0] * matrix_[0][3] + mat.matrix_[3][1] * matrix_[1][3] +
       mat.matrix_[3][2] * matrix_[2][3] + mat.matrix_[3][3] * matrix_[3][3]);

  SetMatrix(tmp);
#endif
  return *this;
}

void TransformationMatrix::MultVecMatrix(double x,
                                         double y,
                                         double& result_x,
                                         double& result_y) const {
  result_x = matrix_[3][0] + x * matrix_[0][0] + y * matrix_[1][0];
  result_y = matrix_[3][1] + x * matrix_[0][1] + y * matrix_[1][1];
  double w = matrix_[3][3] + x * matrix_[0][3] + y * matrix_[1][3];
  if (w != 1 && w != 0) {
    result_x /= w;
    result_y /= w;
  }
}

void TransformationMatrix::MultVecMatrix(double x,
                                         double y,
                                         double z,
                                         double& result_x,
                                         double& result_y,
                                         double& result_z) const {
  result_x =
      matrix_[3][0] + x * matrix_[0][0] + y * matrix_[1][0] + z * matrix_[2][0];
  result_y =
      matrix_[3][1] + x * matrix_[0][1] + y * matrix_[1][1] + z * matrix_[2][1];
  result_z =
      matrix_[3][2] + x * matrix_[0][2] + y * matrix_[1][2] + z * matrix_[2][2];
  double w =
      matrix_[3][3] + x * matrix_[0][3] + y * matrix_[1][3] + z * matrix_[2][3];
  if (w != 1 && w != 0) {
    result_x /= w;
    result_y /= w;
    result_z /= w;
  }
}

bool TransformationMatrix::IsInvertible() const {
  return IsIdentityOrTranslation() || blink::Determinant4x4(matrix_) != 0;
}

TransformationMatrix TransformationMatrix::Inverse() const {
  if (IsIdentityOrTranslation()) {
    // identity matrix
    if (matrix_[3][0] == 0 && matrix_[3][1] == 0 && matrix_[3][2] == 0)
      return TransformationMatrix();

    // translation
    return TransformationMatrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                                -matrix_[3][0], -matrix_[3][1], -matrix_[3][2],
                                1);
  }

  TransformationMatrix inv_mat;
  bool inverted = blink::Inverse(matrix_, inv_mat.matrix_);
  if (!inverted)
    return TransformationMatrix();

  return inv_mat;
}

void TransformationMatrix::MakeAffine() {
  matrix_[0][2] = 0;
  matrix_[0][3] = 0;

  matrix_[1][2] = 0;
  matrix_[1][3] = 0;

  matrix_[2][0] = 0;
  matrix_[2][1] = 0;
  matrix_[2][2] = 1;
  matrix_[2][3] = 0;

  matrix_[3][2] = 0;
  matrix_[3][3] = 1;
}

AffineTransform TransformationMatrix::ToAffineTransform() const {
  return AffineTransform(matrix_[0][0], matrix_[0][1], matrix_[1][0],
                         matrix_[1][1], matrix_[3][0], matrix_[3][1]);
}

void TransformationMatrix::FlattenTo2d() {
  matrix_[2][0] = 0;
  matrix_[2][1] = 0;
  matrix_[0][2] = 0;
  matrix_[1][2] = 0;
  matrix_[2][2] = 1;
  matrix_[3][2] = 0;
  matrix_[2][3] = 0;
}

static inline void BlendFloat(double& from, double to, double progress) {
  if (from != to)
    from = from + (to - from) * progress;
}

bool TransformationMatrix::Is2dTransform() const {
  if (!IsFlat())
    return false;

  // Check perspective.
  if (matrix_[0][3] != 0 || matrix_[1][3] != 0 || matrix_[2][3] != 0 ||
      matrix_[3][3] != 1)
    return false;

  return true;
}

void TransformationMatrix::Blend(const TransformationMatrix& from,
                                 double progress) {
  if (from.IsIdentity() && IsIdentity())
    return;

  if (from.Is2dTransform() && Is2dTransform()) {
    Blend2D(from, progress);
    return;
  }

  // decompose
  DecomposedType from_decomp;
  DecomposedType to_decomp;
  if (!from.Decompose(from_decomp) || !Decompose(to_decomp)) {
    if (progress < 0.5)
      *this = from;
    return;
  }

  // interpolate
  BlendFloat(from_decomp.scale_x, to_decomp.scale_x, progress);
  BlendFloat(from_decomp.scale_y, to_decomp.scale_y, progress);
  BlendFloat(from_decomp.scale_z, to_decomp.scale_z, progress);
  BlendFloat(from_decomp.skew_xy, to_decomp.skew_xy, progress);
  BlendFloat(from_decomp.skew_xz, to_decomp.skew_xz, progress);
  BlendFloat(from_decomp.skew_yz, to_decomp.skew_yz, progress);
  BlendFloat(from_decomp.translate_x, to_decomp.translate_x, progress);
  BlendFloat(from_decomp.translate_y, to_decomp.translate_y, progress);
  BlendFloat(from_decomp.translate_z, to_decomp.translate_z, progress);
  BlendFloat(from_decomp.perspective_x, to_decomp.perspective_x, progress);
  BlendFloat(from_decomp.perspective_y, to_decomp.perspective_y, progress);
  BlendFloat(from_decomp.perspective_z, to_decomp.perspective_z, progress);
  BlendFloat(from_decomp.perspective_w, to_decomp.perspective_w, progress);

  Slerp(&from_decomp.quaternion_x, &to_decomp.quaternion_x, progress);

  // recompose
  Recompose(from_decomp);
}

void TransformationMatrix::Blend2D(const TransformationMatrix& from,
                                   double progress) {
  // Decompose into scale, rotate, translate and skew transforms.
  Decomposed2dType from_decomp;
  Decomposed2dType to_decomp;
  if (!from.Decompose2D(from_decomp) || !Decompose2D(to_decomp)) {
    if (progress < 0.5)
      *this = from;
    return;
  }

  // Take the shorter of the clockwise or counter-clockwise paths.
  double rotation = abs(from_decomp.angle - to_decomp.angle);
  DCHECK(rotation < 2 * M_PI);
  if (rotation > M_PI) {
    if (from_decomp.angle > to_decomp.angle) {
      from_decomp.angle -= 2 * M_PI;
    } else {
      to_decomp.angle -= 2 * M_PI;
    }
  }

  // Interpolate.
  BlendFloat(from_decomp.scale_x, to_decomp.scale_x, progress);
  BlendFloat(from_decomp.scale_y, to_decomp.scale_y, progress);
  BlendFloat(from_decomp.skew_xy, to_decomp.skew_xy, progress);
  BlendFloat(from_decomp.translate_x, to_decomp.translate_x, progress);
  BlendFloat(from_decomp.translate_y, to_decomp.translate_y, progress);
  BlendFloat(from_decomp.angle, to_decomp.angle, progress);

  // Recompose.
  Recompose2D(from_decomp);
}

bool TransformationMatrix::Decompose(DecomposedType& decomp) const {
  if (IsIdentity()) {
    memset(&decomp, 0, sizeof(decomp));
    decomp.perspective_w = 1;
    decomp.scale_x = 1;
    decomp.scale_y = 1;
    decomp.scale_z = 1;
  }

  if (!blink::Decompose(matrix_, decomp))
    return false;
  return true;
}

// Decompose a 2D transformation matrix of the form:
// [m11 m21 0 m41]
// [m12 m22 0 m42]
// [ 0   0  1  0 ]
// [ 0   0  0  1 ]
//
// The decomposition is of the form:
// M = translate * rotate * skew * scale
//     [1 0 0 Tx] [cos(R) -sin(R) 0 0] [1 K 0 0] [Sx 0  0 0]
//   = [0 1 0 Ty] [sin(R)  cos(R) 0 0] [0 1 0 0] [0  Sy 0 0]
//     [0 0 1 0 ] [  0       0    1 0] [0 0 1 0] [0  0  1 0]
//     [0 0 0 1 ] [  0       0    0 1] [0 0 0 1] [0  0  0 1]
//
bool TransformationMatrix::Decompose2D(Decomposed2dType& decomp) const {
  if (!Is2dTransform()) {
    LOG(ERROR) << "2-D decomposition cannot be performed on a 3-D transform.";
    return false;
  }

  double m11 = matrix_[0][0];
  double m21 = matrix_[1][0];
  double m12 = matrix_[0][1];
  double m22 = matrix_[1][1];

  double determinant = m11 * m22 - m12 * m21;
  // Test for matrix being singular.
  if (determinant == 0) {
    return false;
  }

  // Translation transform.
  // [m11 m21 0 m41]    [1 0 0 Tx] [m11 m21 0 0]
  // [m12 m22 0 m42]  = [0 1 0 Ty] [m12 m22 0 0]
  // [ 0   0  1  0 ]    [0 0 1 0 ] [ 0   0  1 0]
  // [ 0   0  0  1 ]    [0 0 0 1 ] [ 0   0  0 1]
  decomp.translate_x = matrix_[3][0];
  decomp.translate_y = matrix_[3][1];

  // For the remainder of the decomposition process, we can focus on the upper
  // 2x2 submatrix
  // [m11 m21] = [cos(R) -sin(R)] [1 K] [Sx 0 ]
  // [m12 m22]   [sin(R)  cos(R)] [0 1] [0  Sy]
  //           = [Sx*cos(R) Sy*(K*cos(R) - sin(R))]
  //             [Sx*sin(R) Sy*(K*sin(R) + cos(R))]

  // Determine sign of the x and y scale.
  decomp.scale_x = 1;
  decomp.scale_y = 1;
  if (determinant < 0) {
    // If the determinant is negative, we need to flip either the x or y scale.
    // Flipping both is equivalent to rotating by 180 degrees.
    // Flip the axis with the minimum unit vector dot product.
    if (m11 < m22) {
      decomp.scale_x = -decomp.scale_x;
    } else {
      decomp.scale_y = -decomp.scale_y;
    }
  }

  // X Scale.
  // m11^2 + m12^2 = Sx^2*(cos^2(R) + sin^2(R)) = Sx^2.
  // Sx = +/-sqrt(m11^2 + m22^2)
  decomp.scale_x *= sqrt(m11 * m11 + m12 * m12);
  m11 /= decomp.scale_x;
  m12 /= decomp.scale_x;

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  Sy*(K*cos(R) - sin(R))]
  // [m12 m22]   [sin(R)  Sy*(K*sin(R) + cos(R))]

  // XY Shear.
  // m11 * m21 + m12 * m22 = Sy*K*cos^2(R) - Sy*sin(R)*cos(R) +
  //                         Sy*K*sin^2(R) + Sy*cos(R)*sin(R)
  //                       = Sy*K
  double scaledShear = m11 * m21 + m12 * m22;
  m21 -= m11 * scaledShear;
  m22 -= m12 * scaledShear;

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  -Sy*sin(R)]
  // [m12 m22]   [sin(R)   Sy*cos(R)]

  // Y Scale.
  // Similar process to determining x-scale.
  decomp.scale_y *= sqrt(m21 * m21 + m22 * m22);
  m21 /= decomp.scale_y;
  m22 /= decomp.scale_y;
  decomp.skew_xy = scaledShear / decomp.scale_y;

  // Rotation transform.
  decomp.angle = atan2(m12, m11);
  return true;
}

void TransformationMatrix::Recompose(const DecomposedType& decomp) {
  MakeIdentity();

  // first apply perspective
  matrix_[0][3] = decomp.perspective_x;
  matrix_[1][3] = decomp.perspective_y;
  matrix_[2][3] = decomp.perspective_z;
  matrix_[3][3] = decomp.perspective_w;

  // now translate
  Translate3d(decomp.translate_x, decomp.translate_y, decomp.translate_z);

  // apply rotation
  double xx = decomp.quaternion_x * decomp.quaternion_x;
  double xy = decomp.quaternion_x * decomp.quaternion_y;
  double xz = decomp.quaternion_x * decomp.quaternion_z;
  double xw = decomp.quaternion_x * decomp.quaternion_w;
  double yy = decomp.quaternion_y * decomp.quaternion_y;
  double yz = decomp.quaternion_y * decomp.quaternion_z;
  double yw = decomp.quaternion_y * decomp.quaternion_w;
  double zz = decomp.quaternion_z * decomp.quaternion_z;
  double zw = decomp.quaternion_z * decomp.quaternion_w;

  // Construct a composite rotation matrix from the quaternion values
  TransformationMatrix rotation_matrix(
      1 - 2 * (yy + zz), 2 * (xy - zw), 2 * (xz + yw), 0, 2 * (xy + zw),
      1 - 2 * (xx + zz), 2 * (yz - xw), 0, 2 * (xz - yw), 2 * (yz + xw),
      1 - 2 * (xx + yy), 0, 0, 0, 0, 1);

  Multiply(rotation_matrix);

  // now apply skew
  if (decomp.skew_yz) {
    TransformationMatrix tmp;
    tmp.SetM32(decomp.skew_yz);
    Multiply(tmp);
  }

  if (decomp.skew_xz) {
    TransformationMatrix tmp;
    tmp.SetM31(decomp.skew_xz);
    Multiply(tmp);
  }

  if (decomp.skew_xy) {
    TransformationMatrix tmp;
    tmp.SetM21(decomp.skew_xy);
    Multiply(tmp);
  }

  // finally, apply scale
  Scale3d(decomp.scale_x, decomp.scale_y, decomp.scale_z);
}

void TransformationMatrix::Recompose2D(const Decomposed2dType& decomp) {
  MakeIdentity();

  // Translate transform.
  SetM41(decomp.translate_x);
  SetM42(decomp.translate_y);

  // Rotate transform.
  double cosAngle = cos(decomp.angle);
  double sinAngle = sin(decomp.angle);
  SetM11(cosAngle);
  SetM21(-sinAngle);
  SetM12(sinAngle);
  SetM22(cosAngle);

  // skew transform.
  if (decomp.skew_xy) {
    TransformationMatrix skewTransform;
    skewTransform.SetM21(decomp.skew_xy);
    Multiply(skewTransform);
  }

  // Scale transform.
  Scale3d(decomp.scale_x, decomp.scale_y, 1);
}

bool TransformationMatrix::IsIntegerTranslation() const {
  if (!IsIdentityOrTranslation())
    return false;

  // Check for translate Z.
  if (matrix_[3][2])
    return false;

  // Check for non-integer translate X/Y.
  if (static_cast<int>(matrix_[3][0]) != matrix_[3][0] ||
      static_cast<int>(matrix_[3][1]) != matrix_[3][1])
    return false;

  return true;
}

// This is the same as gfx::Transform::Preserves2dAxisAlignment().
bool TransformationMatrix::Preserves2dAxisAlignment() const {
  // Check whether an axis aligned 2-dimensional rect would remain axis-aligned
  // after being transformed by this matrix (and implicitly projected by
  // dropping any non-zero z-values).
  //
  // The 4th column can be ignored because translations don't affect axis
  // alignment. The 3rd column can be ignored because we are assuming 2d
  // inputs, where z-values will be zero. The 3rd row can also be ignored
  // because we are assuming 2d outputs, and any resulting z-value is dropped
  // anyway. For the inner 2x2 portion, the only effects that keep a rect axis
  // aligned are (1) swapping axes and (2) scaling axes. This can be checked by
  // verifying only 1 element of every column and row is non-zero.  Degenerate
  // cases that project the x or y dimension to zero are considered to preserve
  // axis alignment.
  //
  // If the matrix does have perspective component that is affected by x or y
  // values: The current implementation conservatively assumes that axis
  // alignment is not preserved.
  bool has_x_or_y_perspective = M14() != 0 || M24() != 0;
  if (has_x_or_y_perspective)
    return false;

  // Use float epsilon here, not double, to round very small rotations back
  // to zero.
  constexpr double kEpsilon = std::numeric_limits<float>::epsilon();

  int num_non_zero_in_row_1 = 0;
  int num_non_zero_in_row_2 = 0;
  int num_non_zero_in_col_1 = 0;
  int num_non_zero_in_col_2 = 0;
  if (std::abs(M11()) > kEpsilon) {
    num_non_zero_in_col_1++;
    num_non_zero_in_row_1++;
  }
  if (std::abs(M12()) > kEpsilon) {
    num_non_zero_in_col_1++;
    num_non_zero_in_row_2++;
  }
  if (std::abs(M21()) > kEpsilon) {
    num_non_zero_in_col_2++;
    num_non_zero_in_row_1++;
  }
  if (std::abs(M22()) > kEpsilon) {
    num_non_zero_in_col_2++;
    num_non_zero_in_row_2++;
  }

  return num_non_zero_in_row_1 <= 1 && num_non_zero_in_row_2 <= 1 &&
         num_non_zero_in_col_1 <= 1 && num_non_zero_in_col_2 <= 1;
}

void TransformationMatrix::ToColumnMajorFloatArray(FloatMatrix4& result) const {
  result[0] = M11();
  result[1] = M12();
  result[2] = M13();
  result[3] = M14();
  result[4] = M21();
  result[5] = M22();
  result[6] = M23();
  result[7] = M24();
  result[8] = M31();
  result[9] = M32();
  result[10] = M33();
  result[11] = M34();
  result[12] = M41();
  result[13] = M42();
  result[14] = M43();
  result[15] = M44();
}

SkMatrix44 TransformationMatrix::ToSkMatrix44(
    const TransformationMatrix& matrix) {
  SkMatrix44 ret(SkMatrix44::kUninitialized_Constructor);
  ret.set4x4(matrix.M11(), matrix.M12(), matrix.M13(), matrix.M14(),
             matrix.M21(), matrix.M22(), matrix.M23(), matrix.M24(),
             matrix.M31(), matrix.M32(), matrix.M33(), matrix.M34(),
             matrix.M41(), matrix.M42(), matrix.M43(), matrix.M44());
  return ret;
}

gfx::Transform TransformationMatrix::ToTransform(
    const TransformationMatrix& matrix) {
  return gfx::Transform(matrix.M11(), matrix.M21(), matrix.M31(), matrix.M41(),
                        matrix.M12(), matrix.M22(), matrix.M32(), matrix.M42(),
                        matrix.M13(), matrix.M23(), matrix.M33(), matrix.M43(),
                        matrix.M14(), matrix.M24(), matrix.M34(), matrix.M44());
}

String TransformationMatrix::ToString(bool as_matrix) const {
  if (as_matrix) {
    // Return as a matrix in row-major order.
    return String::Format(
        "[%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%lg,\n%lg,%lg,%lg,%"
        "lg]",
        M11(), M21(), M31(), M41(), M12(), M22(), M32(), M42(), M13(), M23(),
        M33(), M43(), M14(), M24(), M34(), M44());
  }

  TransformationMatrix::DecomposedType decomposition;
  if (!Decompose(decomposition))
    return ToString(true) + " (degenerate)";

  if (IsIdentityOrTranslation()) {
    if (decomposition.translate_x == 0 && decomposition.translate_y == 0 &&
        decomposition.translate_z == 0)
      return "identity";
    return String::Format("translation(%lg,%lg,%lg)", decomposition.translate_x,
                          decomposition.translate_y, decomposition.translate_z);
  }

  return String::Format(
      "translation(%lg,%lg,%lg), scale(%lg,%lg,%lg), skew(%lg,%lg,%lg), "
      "quaternion(%lg,%lg,%lg,%lg), perspective(%lg,%lg,%lg,%lg)",
      decomposition.translate_x, decomposition.translate_y,
      decomposition.translate_z, decomposition.scale_x, decomposition.scale_y,
      decomposition.scale_z, decomposition.skew_xy, decomposition.skew_xz,
      decomposition.skew_yz, decomposition.quaternion_x,
      decomposition.quaternion_y, decomposition.quaternion_z,
      decomposition.quaternion_w, decomposition.perspective_x,
      decomposition.perspective_y, decomposition.perspective_z,
      decomposition.perspective_w);
}

std::ostream& operator<<(std::ostream& ostream,
                         const TransformationMatrix& transform) {
  return ostream << transform.ToString();
}

static double RoundCloseToZero(double number) {
  return std::abs(number) < 1e-7 ? 0 : number;
}

std::unique_ptr<JSONArray> TransformAsJSONArray(const TransformationMatrix& t) {
  auto array = std::make_unique<JSONArray>();
  {
    auto row = std::make_unique<JSONArray>();
    row->PushDouble(RoundCloseToZero(t.M11()));
    row->PushDouble(RoundCloseToZero(t.M12()));
    row->PushDouble(RoundCloseToZero(t.M13()));
    row->PushDouble(RoundCloseToZero(t.M14()));
    array->PushArray(std::move(row));
  }
  {
    auto row = std::make_unique<JSONArray>();
    row->PushDouble(RoundCloseToZero(t.M21()));
    row->PushDouble(RoundCloseToZero(t.M22()));
    row->PushDouble(RoundCloseToZero(t.M23()));
    row->PushDouble(RoundCloseToZero(t.M24()));
    array->PushArray(std::move(row));
  }
  {
    auto row = std::make_unique<JSONArray>();
    row->PushDouble(RoundCloseToZero(t.M31()));
    row->PushDouble(RoundCloseToZero(t.M32()));
    row->PushDouble(RoundCloseToZero(t.M33()));
    row->PushDouble(RoundCloseToZero(t.M34()));
    array->PushArray(std::move(row));
  }
  {
    auto row = std::make_unique<JSONArray>();
    row->PushDouble(RoundCloseToZero(t.M41()));
    row->PushDouble(RoundCloseToZero(t.M42()));
    row->PushDouble(RoundCloseToZero(t.M43()));
    row->PushDouble(RoundCloseToZero(t.M44()));
    array->PushArray(std::move(row));
  }
  return array;
}

}  // namespace blink
