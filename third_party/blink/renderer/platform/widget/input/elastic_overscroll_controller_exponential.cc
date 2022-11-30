// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller_exponential.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {
#if BUILDFLAG(IS_ANDROID)
constexpr double kRubberbandStiffness = 20;
constexpr double kRubberbandAmplitude = 0.2f;
constexpr double kRubberbandPeriod = 1.1f;
#else
constexpr double kRubberbandStiffness = 20;
constexpr double kRubberbandAmplitude = 0.31f;
constexpr double kRubberbandPeriod = 1.6f;
#endif
}  // namespace

ElasticOverscrollControllerExponential::ElasticOverscrollControllerExponential(
    cc::ScrollElasticityHelper* helper)
    : ElasticOverscrollController(helper) {}

void ElasticOverscrollControllerExponential::DidEnterMomentumAnimatedState() {}

// For these functions which compute the stretch amount, always return a
// rounded value, instead of a floating-point value. The reason for this is
// that Blink's scrolling can become erratic with fractional scroll amounts (in
// particular, if you have a scroll offset of 0.5, Blink will never actually
// bring that value back to 0, which breaks the logic used to determine if a
// layer is pinned in a direction).

gfx::Vector2d ElasticOverscrollControllerExponential::StretchAmountForTimeDelta(
    const base::TimeDelta& delta) const {
  // Compute the stretch amount at a given time after some initial conditions.
  // Do this by first computing an intermediary position given the initial
  // position, initial velocity, time elapsed, and no external forces. Then
  // take the intermediary position and damp it towards zero by multiplying
  // against a negative exponential.
  float amplitude = kRubberbandAmplitude;
  float period = kRubberbandPeriod;
  float critical_dampening_factor =
      expf((-delta.InSecondsF() * kRubberbandStiffness) / period);

  return gfx::ToRoundedVector2d(gfx::ScaleVector2d(
      momentum_animation_initial_stretch_ +
          gfx::ScaleVector2d(momentum_animation_initial_velocity_,
                             delta.InSecondsF() * amplitude),
      critical_dampening_factor));
}

gfx::Vector2d
ElasticOverscrollControllerExponential::StretchAmountForAccumulatedOverscroll(
    const gfx::Vector2dF& accumulated_overscroll) const {
  const float stiffness = std::max(kRubberbandStiffness, 1.0);
  return gfx::ToRoundedVector2d(
      gfx::ScaleVector2d(accumulated_overscroll, 1.0f / stiffness));
}

gfx::Vector2d
ElasticOverscrollControllerExponential::AccumulatedOverscrollForStretchAmount(
    const gfx::Vector2dF& delta) const {
  return gfx::ToRoundedVector2d(
      gfx::ScaleVector2d(delta, kRubberbandStiffness));
}
}  // namespace blink
