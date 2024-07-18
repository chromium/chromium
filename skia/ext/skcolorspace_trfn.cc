// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/skcolorspace_trfn.h"

#include <cmath>

namespace skia {

namespace {

// Solve `y` = `alpha` * `x` for `alpha`. Return false if there is no
// solution, or the solution is 0.
bool GetLinearScale(float x, float y, float& alpha) {
  if (x == 0.f && y == 0.f) {
    alpha = 0.f;
    return true;
  }
  if (x == 0.f || y == 0.f) {
    return false;
  }
  alpha = y / x;
  return true;
}

// Solve `y` = pow(`alpha`, 1/`g`) * `x` for `alpha`. Return false if
// there is no solution, or the solution is 0.
bool GetPowerScale(float x, float y, float g, float& alpha) {
  if (x == 0.f && y == 0.f) {
    alpha = 0.f;
    return true;
  }
  if (x == 0.f || y == 0.f) {
    return false;
  }
  float alpha_to_ginv = y / x;
  alpha = std::pow(alpha_to_ginv, g);
  return true;
}

}  // namespace

skcms_TransferFunction ScaleTransferFunction(const skcms_TransferFunction& f,
                                             float alpha) {
  float alpha_to_ginv = std::pow(alpha, 1 / f.g);
  return {
      f.g, alpha_to_ginv * f.a, alpha_to_ginv * f.b, alpha * f.c,
      f.d, alpha * f.e,         alpha * f.f,
  };
}

bool IsScaledTransferFunction(const skcms_TransferFunction& x,
                              const skcms_TransferFunction& y,
                              float* out_alpha) {
  // The g and d parameters are unaffected by a scale. Require that they be
  // exactly the same.
  if (x.g != y.g) {
    return false;
  }
  if (x.d != y.d) {
    return false;
  }

  // Compute alpha for all all variables. If alpha is 0 then the unscaled
  // parameter was 0 (and so alpha cannot be computed from it).
  float alphas[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
  if (!GetLinearScale(x.c, y.c, alphas[0])) {
    return false;
  }
  if (!GetLinearScale(x.e, y.e, alphas[1])) {
    return false;
  }
  if (!GetLinearScale(x.f, y.f, alphas[2])) {
    return false;
  }
  if (!GetPowerScale(x.a, y.a, x.g, alphas[3])) {
    return false;
  }
  if (!GetPowerScale(x.b, y.b, x.g, alphas[4])) {
    return false;
  }

  // Ensure all non-zero alphas are close to each other.
  constexpr float kEpsilon = 1e-5f;
  float alpha = 0.f;
  for (size_t i = 0; i < 5; ++i) {
    if (alphas[i] == 0.f) {
      continue;
    }
    if (alpha == 0.f) {
      alpha = alphas[i];
      continue;
    }
    if (std::abs(alpha - alphas[i]) > kEpsilon) {
      return false;
    }
  }

  // Scaling by zero will just cause bugs, so reject it.
  if (alpha == 0.f) {
    return false;
  }

  // Return the result.
  if (out_alpha) {
    *out_alpha = alpha;
  }
  return true;
}

}  // namespace skia
