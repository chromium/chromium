// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CLIENT_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
struct DOMPaintTimingInfo;
struct ElementTimingInfo;
class ImageRecord;
class LayoutObject;
class MediaTiming;
class TextRecord;

// `PaintTimingClient` is the interface PaintTiming uses to communicate
// contentful element and document-level paint events to clients.
class PaintTimingClient : public GarbageCollectedMixin {
 public:
  virtual ~PaintTimingClient() = default;

  // Called when the first contentful paint for an image type element (<img> SVG
  // image, poster image, or first video frame) has been observed. The size is
  // guaranteed to be non-zero. This is called regardless of whether the image
  // is sufficiently loaded or the first animated frame has been painted.
  virtual void OnElementFirstContentfulPaint(ImageRecord*) {}

  // Called when the last, "sufficiently loaded" contentful paint for an image
  // type element has been observed, which indicates the `ImageRecord` is ready
  // for paint timing. Subsequent contentful paints for the element will be
  // ignored unless the underlying image changes, in which case paint tracking
  // restarts with a new `ImageRecord`.
  virtual void OnElementLastContentfulPaint(ImageRecord*) {}

  // Called when a text aggregating node is being painted and is ready for paint
  // timing (opacity > 0, etc.). Unlike for `ImageRecord`s, this can be called
  // if the size is 0, e.g. if the text element is just out of the viewport.
  //
  // Note that text elements may be re-reported when the text content changes if
  // caused by an interaction; `was_previously_reported` will be true in this
  // case.
  virtual void OnElementLastContentfulPaint(TextRecord*,
                                            bool was_previously_reported) {}

  // Called when an image has been removed. Clients typically don't need to
  // override this unless caching the `ImageRecord`.
  virtual void OnImageRemoved(const LayoutObject&, const MediaTiming*) {}

  // Called when the paint phase has finished.
  virtual void OnPaintFinished() {}

  // Called when paint and presentation time is available for the given image
  // and text records.
  //
  // TODO(crbug.com/535432431): Remove the ElementTimingInfo vector once
  // ImageElementTiming uses ImagePaintTimingDetector.
  virtual void OnFramePresented(const HeapVector<Member<ImageRecord>>&,
                                const HeapVector<Member<TextRecord>>&,
                                const HeapVector<Member<ElementTimingInfo>>&,
                                const DOMPaintTimingInfo&) {}

  // Called when a discrete input or non-programmatic scroll has been detected.
  virtual void OnInputOrScroll() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_CLIENT_H_
