// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_UTIL_H_

#include <memory>

#include "base/optional.h"
#include "cc/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"

namespace blink {

using CompositorScrollTimeline = cc::ScrollTimeline;

class AnimationTimeline;
class ComputedStyle;
class Node;

namespace scroll_timeline_util {

// Converts the input timeline to the compositor representation of a
// ScrollTimeline. Returns nullptr if the input is not a ScrollTimeline.
scoped_refptr<CompositorScrollTimeline> CORE_EXPORT
ToCompositorScrollTimeline(AnimationTimeline*);

// Retrieves the 'scroll' compositor element id for the input node, or
// base::nullopt if it does not exist.
base::Optional<CompositorElementId> CORE_EXPORT
GetCompositorScrollElementId(const Node*);

// Convert the blink concept of a ScrollTimeline orientation into the cc one.
//
// This implements a subset of the conversions documented in
// https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
CompositorScrollTimeline::ScrollDirection CORE_EXPORT
ConvertOrientation(ScrollTimeline::ScrollDirection, const ComputedStyle*);

double ComputeProgress(double current_offset,
                       const WTF::Vector<double>& resolved_offsets);

}  // namespace scroll_timeline_util

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_UTIL_H_
