// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_

#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {
class PaintLayer;
class IntRect;
class LayoutObject;
class TracedValue;
class LocalFrameView;
class LayoutImage;

class ImageRecord : public base::SupportsWeakPtr<ImageRecord> {
 public:
  DOMNodeId node_id = kInvalidDOMNodeId;
  double first_size = 0.0;
  // LastImagePaint uses the order of the first paints to determine the last
  // image.
  unsigned first_paint_index = 0;
  unsigned frame_index = 0;
  base::TimeTicks first_paint_time_after_loaded = base::TimeTicks();
  bool loaded = false;
  String image_url = "";
};

// ImagePaintTimingDetector contains Largest Image Paint and Last Image Paint.
//
// Largest Image Paint timing measures when the largest image element within
// viewport finishes painting. Last Image Paint timing measures when the last
// image element within viewport finishes painting. Specifically, they:
// 1. Tracks all images' first invalidation, recording their visual size, if
// this image is within viewport.
// 2. When an image finishes loading, record its paint time.
// 3. At the end of each prepaint tree walk, the algorithm starts an analysis.
// In the analysis:
// 3.1 Largest Image Paint finds the largest image by the first visual size. If
// it has finished loading, reports a candidate result as its first paint time
// since loaded.
// 3.2 Last Image Paint finds the latest image by images' first paint time
// (regardless of loaded or not), reports a candidate result as its first paint
// time since loaded.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Image Paint candidate and Last Image Paint candidate respectively as
// their final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.1k2rnrs6mdmt
class CORE_EXPORT ImagePaintTimingDetector final
    : public GarbageCollectedFinalized<ImagePaintTimingDetector> {
  friend class ImagePaintTimingDetectorTest;

 public:
  ImagePaintTimingDetector(LocalFrameView* frame_view);
  void RecordImage(const LayoutObject& object,
                   const PaintLayer& painting_layer);
  void OnPrePaintFinished();
  void NotifyNodeRemoved(DOMNodeId);
  base::TimeTicks LargestImagePaint() const { return largest_image_paint_; }
  base::TimeTicks LastImagePaint() const { return last_image_paint_; }
  void Trace(blink::Visitor*);

 private:
  ImageRecord* FindLargestPaintCandidate();
  ImageRecord* FindLastPaintCandidate();
  void PopulateTraceValue(TracedValue& value,
                          const ImageRecord& first_image_paint,
                          unsigned report_count) const;
  IntRect CalculateTransformedRect(LayoutRect& visual_rect,
                                   const PaintLayer& painting_layer) const;
  // This is provided for unit test to force invoking swap promise callback.
  void ReportSwapTime(unsigned max_frame_index_to_time,
                      WebLayerTreeView::SwapResult,
                      base::TimeTicks);
  void RegisterNotifySwapTime();
  void OnLargestImagePaintDetected(const ImageRecord&);
  void OnLastImagePaintDetected(const ImageRecord&);

  bool IsJustLoaded(const LayoutImage*, const ImageRecord&) const;
  void Analyze();

  base::RepeatingCallback<void(WebLayerTreeView::ReportTimeCallback)>
      notify_swap_time_override_for_testing_;

  HashSet<DOMNodeId> size_zero_ids_;
  HashMap<DOMNodeId, std::unique_ptr<ImageRecord>> id_record_map_;
  std::priority_queue<base::WeakPtr<ImageRecord>,
                      std::vector<base::WeakPtr<ImageRecord>>,
                      bool (*)(const base::WeakPtr<ImageRecord>&,
                               const base::WeakPtr<ImageRecord>&)>
      largest_image_heap_;
  std::priority_queue<base::WeakPtr<ImageRecord>,
                      std::vector<base::WeakPtr<ImageRecord>>,
                      bool (*)(const base::WeakPtr<ImageRecord>&,
                               const base::WeakPtr<ImageRecord>&)>
      latest_image_heap_;
  unsigned recorded_node_count_ = 0;

  // Node-ids of records pending swap time are stored in this queue until they
  // get a swap time.
  std::queue<DOMNodeId> records_pending_timing_;

  // Used to report the last candidates of each metric
  unsigned largest_image_candidate_index_max_ = 0;
  unsigned last_image_candidate_index_max_ = 0;

  // Used to decide the last image
  unsigned first_paint_index_max_ = 0;

  // Used to decide which frame a record belongs to.
  unsigned frame_index_ = 1;

  unsigned last_frame_index_queued_for_timing_ = 0;

  base::TimeTicks largest_image_paint_;
  base::TimeTicks last_image_paint_;
  Member<LocalFrameView> frame_view_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_PAINT_TIMING_DETECTOR_H_
