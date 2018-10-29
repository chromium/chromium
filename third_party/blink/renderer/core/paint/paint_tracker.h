// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class PaintLayer;
class TextPaintTimingDetector;
class ImagePaintTimingDetector;

// PaintTracker contains some of paint metric detectors, providing common
// infrastructure for these detectors.
//
// Users has to enable 'loading' trace category to enable the metrics.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit
class CORE_EXPORT PaintTracker : public GarbageCollected<PaintTracker> {
 public:
  PaintTracker(LocalFrameView*);
  void NotifyObjectPrePaint(const LayoutObject& object,
                            const PaintLayer& painting_layer);
  void NotifyNodeRemoved(const LayoutObject& object);
  void NotifyPrePaintFinished();
  void DidChangePerformanceTiming();
  void Dispose();

  TextPaintTimingDetector& GetTextPaintTimingDetector() {
    return *text_paint_timing_detector_;
  }
  ImagePaintTimingDetector& GetImagePaintTimingDetector() {
    return *image_paint_timing_detector_;
  }
  void Trace(Visitor* visitor);

 private:
  Member<LocalFrameView> frame_view_;
  Member<TextPaintTimingDetector> text_paint_timing_detector_;
  Member<ImagePaintTimingDetector> image_paint_timing_detector_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TRACKER_H_
