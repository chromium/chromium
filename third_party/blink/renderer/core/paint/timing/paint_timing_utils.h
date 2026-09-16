// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace cc {
class HeadsUpDisplayLayer;
}

namespace blink {
class Document;
class LocalDOMWindow;
class LocalFrameView;
}  // namespace blink

namespace blink::paint_timing {

// Returns true if `object` will cause an image to be rendered, and false
// otherwise.
inline bool CORE_EXPORT IsImageType(const LayoutObject& object) {
  return object.IsImage() || object.IsSVGImage() || object.IsVideo() ||
         object.StyleRef().HasBackgroundImage();
}

// Returns true if `node` is considered text, and false otherwise.
inline bool CORE_EXPORT IsTextType(const Node& node) {
  return node.IsTextNode();
}

CORE_EXPORT cc::HeadsUpDisplayLayer* GetHUDLayerIfContentfulPaintRectsEnabled(
    LocalFrameView* frame_view);

CORE_EXPORT cc::HeadsUpDisplayLayer* GetHUDLayerIfLayoutShiftRectsEnabled(
    LocalFrameView* frame_view);

// Notifies the document loader that performance timing has changed in some way
// if the window, document, and loader are non-null. Causes the current
// performance timing values to be sent to UKM.
CORE_EXPORT void NotifyLoaderPerformanceTimingChanged(LocalDOMWindow*);
CORE_EXPORT void NotifyLoaderPerformanceTimingChanged(Document*);

}  // namespace blink::paint_timing

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_UTILS_H_
