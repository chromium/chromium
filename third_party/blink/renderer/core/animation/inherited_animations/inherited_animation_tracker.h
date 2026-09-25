// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INHERITED_ANIMATIONS_INHERITED_ANIMATION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INHERITED_ANIMATIONS_INHERITED_ANIMATION_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class LayoutObject;

// Tracks animations whose animated value another element inherits. The
// compositor cannot keep the inheriting element in sync, so these run on the
// main thread. No-op unless `TrackAnimatedSources` is enabled.
class CORE_EXPORT InheritedAnimationTracker final {
  STATIC_ONLY(InheritedAnimationTracker);

 public:
  // Marks the object for a paint property update if the animated sources it
  // inherits changed. `old_style` may be null.
  static void StyleDidChange(LayoutObject&, const ComputedStyle* old_style);

  // Records unsupported inheritance on each ancestor whose animated value the
  // object inherits, downgrading its composited animations of that property
  // the first time. Called in pre-paint, after the paint property update.
  static void DowngradeForUnsupportedInheritance(const LayoutObject&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INHERITED_ANIMATIONS_INHERITED_ANIMATION_TRACKER_H_
