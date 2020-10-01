// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_ELASTIC_OVERSCROLL_CONTROLLER_BEZIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_ELASTIC_OVERSCROLL_CONTROLLER_BEZIER_H_

#include "base/macros.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller.h"
#include "ui/gfx/geometry/cubic_bezier.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {
// Manages scroller stretch and rebounds when overscrolling. This controller
// uses a Bezier curve.
class PLATFORM_EXPORT ElasticOverscrollControllerBezier
    : public ElasticOverscrollController {
 public:
  explicit ElasticOverscrollControllerBezier(
      cc::ScrollElasticityHelper* helper);
  ~ElasticOverscrollControllerBezier() override = default;

  void DidEnterMomentumAnimatedState() override;
  gfx::Vector2d StretchAmountForTimeDelta(
      const base::TimeDelta& delta) const override;
  gfx::Vector2d StretchAmountForAccumulatedOverscroll(
      const gfx::Vector2dF& accumulated_overscroll) const override;
  gfx::Vector2d AccumulatedOverscrollForStretchAmount(
      const gfx::Vector2dF& delta) const override;
  gfx::Vector2dF OverscrollBoundary(const gfx::Size& scroller_bounds) const;
  double StretchAmountForForwardBounce(
      const base::TimeDelta& delta,
      const base::TimeDelta& bounce_forwards_duration,
      const double velocity,
      const double initial_stretch,
      const double bounce_forwards_distance) const;
  double StretchAmountForBackwardBounce(
      const base::TimeDelta& delta,
      const base::TimeDelta& bounce_backwards_duration,
      const double bounce_forwards_distance) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ElasticOverscrollControllerBezierTest,
                           VerifyInitialStretchDelta);
  FRIEND_TEST_ALL_PREFIXES(ElasticOverscrollControllerBezierTest,
                           VerifyForwardAnimationIsNotPlayed);

  const gfx::CubicBezier bounce_forwards_curve_;
  base::TimeDelta bounce_forwards_duration_x_;
  base::TimeDelta bounce_forwards_duration_y_;
  gfx::Vector2dF bounce_forwards_distance_;

  const gfx::CubicBezier bounce_backwards_curve_;
  base::TimeDelta bounce_backwards_duration_x_;
  base::TimeDelta bounce_backwards_duration_y_;
  DISALLOW_COPY_AND_ASSIGN(ElasticOverscrollControllerBezier);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_ELASTIC_OVERSCROLL_CONTROLLER_BEZIER_H_
