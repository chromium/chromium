// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_DELEGATE_H_

#include <memory>

#include "cc/animation/animation_curve.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT CompositorAnimationDelegate {
 public:
  virtual ~CompositorAnimationDelegate() = default;

  // TODO(yigu): The Notify* methods should be called from cc once per
  // animation.
  virtual void NotifyAnimationStarted(double monotonic_time, int group) = 0;
  virtual void NotifyAnimationFinished(double monotonic_time, int group) = 0;
  virtual void NotifyAnimationAborted(double monotonic_time, int group) = 0;
  // In the current state of things, notifyAnimationTakeover only applies to
  // scroll offset animations since main thread scrolling reasons can be added
  // while the compositor is animating. Keeping this non-pure virtual since
  // it doesn't apply to CSS animations.
  virtual void NotifyAnimationTakeover(
      double monotonic_time,
      double animation_start_time,
      std::unique_ptr<cc::AnimationCurve> curve) {}
  virtual void NotifyLocalTimeUpdated(
      base::Optional<base::TimeDelta> local_time) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_DELEGATE_H_
