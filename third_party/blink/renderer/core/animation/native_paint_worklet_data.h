// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_NATIVE_PAINT_WORKLET_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_NATIVE_PAINT_WORKLET_DATA_H_

#include "base/memory/ref_counted.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class CORE_EXPORT NativePaintWorkletData
    : public GarbageCollected<NativePaintWorkletData> {
 public:
  NativePaintWorkletData(Element* element,
                         Animation::NativePaintWorkletProperties property)
      : element_(element), npw_property_(property) {}

  enum class CompositedPaintStatus {
    // A fresh compositing decision is required for an animated property.
    // Any style change for the corresponding property requires paint
    // invalidation. Even if rendered by a composited animation, we need to
    // trigger repaint in order to set up a worklet paint image. If the property
    // is animated, paint will decide if the animation is composited and will
    // update the status accordingly.
    kNeedsRepaint = 0,

    // An animation is affecting the target property, but it is not being
    // composited. Paint can short-circuit setting up a worklet paint image
    // since it is not required. Any style change affecting the target property
    // requires repaint, but no new compositing decision.
    kNotComposited = 1,

    // An animation affecting the target property is being rendered on the
    // compositor. Though repaint won't get triggered by a change to the
    // property, it can still be triggered for other reasons, in which case a
    // worklet paint image must be generated.
    kComposited = 2,

    // No animation affects the targeted property, so no paint invalidation or
    // image generation is required.
    kNoAnimation = 3
  };

  CompositedPaintStatus GetCompositedPaintStatus() const {
    return composited_paint_status_;
  }

  void SetUpdateTriggersPaintPropertyUpdate(bool state) {
    update_triggers_paint_property_update_ = state;
  }

  bool UpdateCompositedPaintStatus(
      Animation::NativePaintWorkletReasons aggregated_reasons,
      Animation::NativePaintWorkletReasons overlapping_reasons);

  bool SetStatus(CompositedPaintStatus status);

  void MaybeSetNeedsKeyframeSnapshot(const Element& element,
                                     const ComputedStyle& new_style,
                                     bool forced_update);
  void SetNeedsKeyframeSnapshot();

  void SetAnimation(Animation* animation);

  Animation* GetAnimation() const { return animation_; }

  scoped_refptr<CompositorAnimationCurve> GetAnimationCurve();

  void SetAnimationCurve(scoped_refptr<CompositorAnimationCurve> curve) {
    animation_curve_ = animation_ ? std::move(curve) : nullptr;
    needs_keyframes_snapshot_update_ = false;
  }

  bool NeedsKeyframeSnapshotUpdate() {
    return needs_keyframes_snapshot_update_;
  }

  // The final compositing decision is made during Animation::Precommit.
  // Even if the [Pre]Paint decision is kComposited, this is a recommendation
  // and not a mandate. If the animation is not composited, the compositor
  // paint status must be updated accordingly. A "no" from Precommit can still
  // leave the status as kNeedsRepaint, as long as the reason for the "no"
  // vote is having no visual effect. The next paint will either create a
  // deferred paint image and trigger a new compositing decision or downgrade
  // the status.
  void NotifyNotComposited(Animation* animation);

  void Trace(Visitor*) const;

 private:
  void TriggerPaintInvalidation();

  WeakMember<Element> element_;
  Animation::NativePaintWorkletProperties npw_property_;
  CompositedPaintStatus composited_paint_status_ =
      CompositedPaintStatus::kNoAnimation;
  bool update_triggers_paint_property_update_ = false;
  WeakMember<Animation> animation_;
  scoped_refptr<CompositorAnimationCurve> animation_curve_;
  bool needs_keyframes_snapshot_update_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_NATIVE_PAINT_WORKLET_DATA_H_
