// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_

#include <stdint.h>
#include <vector>
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CompositingReasons = uint64_t;

#define FOR_EACH_COMPOSITING_REASON(V)                                        \
  /* Intrinsic reasons that can be known right away by the layer. */          \
  V(3DTransform)                                                              \
  V(Trivial3DTransform)                                                       \
  V(Video)                                                                    \
  V(Canvas)                                                                   \
  V(Plugin)                                                                   \
  V(IFrame)                                                                   \
  V(SVGRoot)                                                                  \
  V(BackfaceVisibilityHidden)                                                 \
  V(ActiveTransformAnimation)                                                 \
  V(ActiveOpacityAnimation)                                                   \
  V(ActiveFilterAnimation)                                                    \
  V(ActiveBackdropFilterAnimation)                                            \
  V(ScrollDependentPosition)                                                  \
  V(OverflowScrolling)                                                        \
  V(OverflowScrollingParent)                                                  \
  V(OutOfFlowClipping)                                                        \
  V(VideoOverlay)                                                             \
  V(WillChangeTransform)                                                      \
  V(WillChangeOpacity)                                                        \
  V(WillChangeFilter)                                                         \
  V(WillChangeBackdropFilter)                                                 \
  /* Reasons that depend on ancestor properties */                            \
  V(BackfaceInvisibility3DAncestor)                                           \
  /* This flag is needed only when none of the explicit kWillChange* reasons  \
     are set. */                                                              \
  V(WillChangeOther)                                                          \
  V(BackdropFilter)                                                           \
  V(RootScroller)                                                             \
  V(XrOverlay)                                                                \
  V(Viewport)                                                                 \
                                                                              \
  /* Overlap reasons that require knowing what's behind you in paint-order    \
     before knowing the answer. */                                            \
  V(AssumedOverlap)                                                           \
  V(Overlap)                                                                  \
  V(NegativeZIndexChildren)                                                   \
  V(SquashingDisallowed)                                                      \
                                                                              \
  /* Subtree reasons that require knowing what the status of your subtree is  \
     before knowing the answer. */                                            \
  V(OpacityWithCompositedDescendants)                                         \
  V(MaskWithCompositedDescendants)                                            \
  V(ReflectionWithCompositedDescendants)                                      \
  V(FilterWithCompositedDescendants)                                          \
  V(BlendingWithCompositedDescendants)                                        \
  V(PerspectiveWith3DDescendants)                                             \
  V(Preserve3DWith3DDescendants)                                              \
  V(IsolateCompositedDescendants)                                             \
  V(FullscreenVideoWithCompositedDescendants)                                 \
                                                                              \
  /* The root layer is a special case. It may be forced to be a layer, but it \
  also needs to be a layer if anything else in the subtree is composited. */  \
  V(Root)                                                                     \
                                                                              \
  /* CompositedLayerMapping internal hierarchy reasons. Some of them are also \
  used in CompositeAfterPaint. */                                             \
  V(LayerForHorizontalScrollbar)                                              \
  V(LayerForVerticalScrollbar)                                                \
  V(LayerForScrollCorner)                                                     \
  V(LayerForScrollingContents)                                                \
  V(LayerForSquashingContents)                                                \
  V(LayerForForeground)                                                       \
  V(LayerForMask)                                                             \
  /* Composited layer painted on top of all other layers as decoration. */    \
  V(LayerForDecoration)                                                       \
  /* Used in CompositeAfterPaint for link highlight, frame overlay, etc. */   \
  V(LayerForOther)

class PLATFORM_EXPORT CompositingReason {
  DISALLOW_NEW();

 private:
  // This contains ordinal values for compositing reasons and will be used to
  // generate the compositing reason bits.
  enum {
#define V(name) kE##name,
    FOR_EACH_COMPOSITING_REASON(V)
#undef V
  };

#define V(name) static_assert(kE##name < 64, "Should fit in 64 bits");
  FOR_EACH_COMPOSITING_REASON(V)
#undef V

 public:
  static std::vector<const char*> ShortNames(CompositingReasons);
  static std::vector<const char*> Descriptions(CompositingReasons);
  static String ToString(CompositingReasons);

  enum : CompositingReasons {
    kNone = 0,
    kAll = ~static_cast<CompositingReasons>(0),
#define V(name) k##name = UINT64_C(1) << kE##name,
    FOR_EACH_COMPOSITING_REASON(V)
#undef V

    // Various combinations of compositing reasons are defined here also, for
    // more intuitive and faster bitwise logic.
    kComboActiveAnimation =
        kActiveTransformAnimation | kActiveOpacityAnimation |
        kActiveFilterAnimation | kActiveBackdropFilterAnimation,

    kComboAllDirectStyleDeterminedReasons =
        k3DTransform | kTrivial3DTransform | kBackfaceVisibilityHidden |
        kComboActiveAnimation | kWillChangeTransform | kWillChangeOpacity |
        kWillChangeFilter | kWillChangeOther | kBackdropFilter |
        kWillChangeBackdropFilter,

    kComboAllDirectNonStyleDeterminedReasons =
        kVideo | kCanvas | kPlugin | kIFrame | kSVGRoot |
        kOverflowScrollingParent | kOutOfFlowClipping | kVideoOverlay |
        kXrOverlay | kRoot | kRootScroller | kScrollDependentPosition |
        kBackfaceInvisibility3DAncestor,

    kComboAllDirectReasons = kComboAllDirectStyleDeterminedReasons |
                             kComboAllDirectNonStyleDeterminedReasons,

    kComboAllCompositedScrollingDeterminedReasons =
        kScrollDependentPosition | kOverflowScrolling,

    kComboCompositedDescendants =
        kIsolateCompositedDescendants | kOpacityWithCompositedDescendants |
        kMaskWithCompositedDescendants | kFilterWithCompositedDescendants |
        kBlendingWithCompositedDescendants |
        kReflectionWithCompositedDescendants,

    kCombo3DDescendants =
        kPreserve3DWith3DDescendants | kPerspectiveWith3DDescendants,

    kComboAllStyleDeterminedReasons = kComboAllDirectStyleDeterminedReasons |
                                      kComboCompositedDescendants |
                                      kCombo3DDescendants,

    kComboSquashableReasons =
        kOverlap | kAssumedOverlap | kOverflowScrollingParent,

    kPreventingSubpixelAccumulationReasons =
        kWillChangeTransform | kActiveTransformAnimation,

    kDirectReasonsForPaintOffsetTranslationProperty =
        kScrollDependentPosition | kVideo | kCanvas | kPlugin | kIFrame |
        kSVGRoot,
    kDirectReasonsForTransformProperty =
        k3DTransform | kTrivial3DTransform | kWillChangeTransform |
        kWillChangeOther | kPerspectiveWith3DDescendants |
        kPreserve3DWith3DDescendants | kActiveTransformAnimation,
    kDirectReasonsForScrollTranslationProperty =
        kRootScroller | kOverflowScrolling,
    kDirectReasonsForEffectProperty =
        kActiveOpacityAnimation | kWillChangeOpacity | kBackdropFilter |
        kWillChangeBackdropFilter | kActiveBackdropFilterAnimation,
    kDirectReasonsForFilterProperty =
        kActiveFilterAnimation | kWillChangeFilter,
  };
};

// Any reasons other than overlap or assumed overlap will require the layer to
// be separately compositing.
inline bool RequiresCompositing(CompositingReasons reasons) {
  return reasons & ~CompositingReason::kComboSquashableReasons;
}

// If the layer has overlap or assumed overlap, but no other reasons, then it
// should be squashed.
inline bool RequiresSquashing(CompositingReasons reasons) {
  return !RequiresCompositing(reasons) &&
         (reasons & CompositingReason::kComboSquashableReasons);
}

struct CompositingReasonsStats {
  size_t overlap_layers = 0;
  size_t active_animation_layers = 0;
  size_t assumed_overlap_layers = 0;
  size_t indirect_composited_layers = 0;
  size_t total_composited_layers = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_REASONS_H_
