// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/animated_color.h"

#include "third_party/blink/renderer/core/animation/inherited_animations/deferred_property_utils.h"

namespace blink {

cc::PaintColor AnimatedColor::ResolvePaintColor() const {
  const cc::DeferredPropertyKey key = ToDeferredPropertyKey(source, property);
  return {
      /*color=*/value.toSkColor4f(),
      /*key=*/key,
      // Untracked dependencies alter the color, so raster cannot directly
      // substitute the source's animated value.
      /*needs_main_thread_repaint=*/key && source.has_untracked_dependencies,
  };
}

}  // namespace blink
