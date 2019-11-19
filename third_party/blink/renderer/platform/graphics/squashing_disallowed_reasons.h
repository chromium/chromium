// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SQUASHING_DISALLOWED_REASONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SQUASHING_DISALLOWED_REASONS_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using SquashingDisallowedReasons = unsigned;

#define FOR_EACH_SQUASHING_DISALLOWED_REASON(V) \
  V(ScrollsWithRespectToSquashingLayer)         \
  V(SquashingSparsityExceeded)                  \
  V(ClippingContainerMismatch)                  \
  V(OpacityAncestorMismatch)                    \
  V(TransformAncestorMismatch)                  \
  V(FilterMismatch)                             \
  V(WouldBreakPaintOrder)                       \
  V(SquashingVideoIsDisallowed)                 \
  V(SquashingLayoutEmbeddedContentIsDisallowed) \
  V(SquashingBlendingIsDisallowed)              \
  V(NearestFixedPositionMismatch)               \
  V(ScrollChildWithCompositedDescendants)       \
  V(SquashingLayerIsAnimating)                  \
  V(RenderingContextMismatch)                   \
  V(FragmentedContent)                          \
  V(ClipPathMismatch)                           \
  V(MaskMismatch)                               \
  V(CrossesLayoutContainmentBoundary)

class PLATFORM_EXPORT SquashingDisallowedReason {
  DISALLOW_NEW();

 private:
  // This contains ordinal values for squashing disallowed reasons and will be
  // used to generate the squashing disallowed reason bits.
  enum {
#define V(name) kE##name,
    FOR_EACH_SQUASHING_DISALLOWED_REASON(V)
#undef V
  };

#define V(name) static_assert(kE##name < 32, "Should fit in 32 bits");
  FOR_EACH_SQUASHING_DISALLOWED_REASON(V)
#undef V

 public:
  static Vector<const char*> ShortNames(SquashingDisallowedReasons);
  static Vector<const char*> Descriptions(SquashingDisallowedReasons);

  enum : SquashingDisallowedReasons {
    kNone = 0,
#define V(name) k##name = 1u << kE##name,
    FOR_EACH_SQUASHING_DISALLOWED_REASON(V)
#undef V
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SQUASHING_DISALLOWED_REASONS_H_
