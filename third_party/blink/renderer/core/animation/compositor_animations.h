/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATIONS_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Animation;
class CompositorAnimation;
class Element;
class KeyframeEffectModelBase;
class PaintArtifactCompositor;

class CORE_EXPORT CompositorAnimations {
  STATIC_ONLY(CompositorAnimations);

 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  using FailureReasons = uint32_t;
  enum FailureReason : uint32_t {
    kNoFailure = 0,

    // Cases where the compositing is disabled by an exterior cause.
    kAcceleratedAnimationsDisabled = 1 << 0,
    kEffectSuppressedByDevtools = 1 << 1,

    // There are many cases where an animation may not be valid (e.g. it is not
    // playing, or has no effect, etc). In these cases we would never composite
    // it in any world, so we lump them together.
    kInvalidAnimationOrEffect = 1 << 2,

    // The compositor is not able to support all setups of timing values; see
    // CompositorAnimations::ConvertTimingForCompositor.
    kEffectHasUnsupportedTimingParameters = 1 << 3,

    // Currently the compositor does not support any composite mode other than
    // 'replace'.
    kEffectHasNonReplaceCompositeMode = 1 << 4,

    // Cases where the target element isn't in a valid compositing state.
    kTargetHasInvalidCompositingState = 1 << 5,

    // Cases where the target is invalid (but that we could feasibly address).
    kTargetHasIncompatibleAnimations = 1 << 6,
    kTargetHasCSSOffset = 1 << 7,
    kTargetHasMultipleTransformProperties = 1 << 8,

    // Cases relating to the properties being animated.
    kAnimationAffectsNonCSSProperties = 1 << 9,
    kTransformRelatedPropertyCannotBeAcceleratedOnTarget = 1 << 10,
    kTransformRelatedPropertyDependsOnBoxSize = 1 << 11,
    kFilterRelatedPropertyMayMovePixels = 1 << 12,
    kUnsupportedCSSProperty = 1 << 13,
    kMultipleTransformAnimationsOnSameTarget = 1 << 14,
    kMixedKeyframeValueTypes = 1 << 15,

    // The maximum number of flags in this enum (excluding itself). New flags
    // should increment this number but it should never be decremented because
    // the values are used in UMA histograms. It should also be noted that it
    // excludes the kNoFailure value.
    kFailureReasonCount = 16,
  };

  static FailureReasons CheckCanStartAnimationOnCompositor(
      const Timing&,
      const Element&,
      const Animation*,
      const EffectModel&,
      const PaintArtifactCompositor*,
      double animation_playback_rate);
  static void CancelIncompatibleAnimationsOnCompositor(const Element&,
                                                       const Animation&,
                                                       const EffectModel&);
  static void StartAnimationOnCompositor(
      const Element&,
      int group,
      base::Optional<double> start_time,
      double time_offset,
      const Timing&,
      const Animation*,
      CompositorAnimation&,
      const EffectModel&,
      Vector<int>& started_keyframe_model_ids,
      double animation_playback_rate);
  static void CancelAnimationOnCompositor(const Element&,
                                          CompositorAnimation*,
                                          int id);
  static void PauseAnimationForTestingOnCompositor(const Element&,
                                                   const Animation&,
                                                   int id,
                                                   double pause_time);

  static void AttachCompositedLayers(Element&, CompositorAnimation*);

  struct CompositorTiming {
    Timing::PlaybackDirection direction;
    AnimationTimeDelta scaled_duration;
    double scaled_time_offset;
    double adjusted_iteration_count;
    double playback_rate;
    Timing::FillMode fill_mode;
    double iteration_start;
  };

  static bool ConvertTimingForCompositor(const Timing&,
                                         double time_offset,
                                         CompositorTiming& out,
                                         double animation_playback_rate);

  static void GetAnimationOnCompositor(
      const Element&,
      const Timing&,
      int group,
      base::Optional<double> start_time,
      double time_offset,
      const KeyframeEffectModelBase&,
      Vector<std::unique_ptr<CompositorKeyframeModel>>& animations,
      double animation_playback_rate);

  static CompositorElementIdNamespace CompositorElementNamespaceForProperty(
      CSSPropertyID property);

 private:
  static FailureReasons CheckCanStartEffectOnCompositor(
      const Timing&,
      const Element&,
      const Animation*,
      const EffectModel&,
      const PaintArtifactCompositor*,
      double animation_playback_rate);
  static FailureReasons CheckCanStartElementOnCompositor(const Element&);

  friend class AnimationCompositorAnimationsTest;
};

}  // namespace blink

#endif
