// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_PAINT_TIMING_DETECTOR_H_

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/text_element_timing.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
class LayoutBoxModelObject;
class LocalFrameView;
class PaintTimingCallbackManager;
class PropertyTreeStateOrAlias;
class TextElementTiming;
struct DOMPaintTimingInfo;
class SoftNavigationContext;

class CORE_EXPORT LargestTextPaintManager final {
  DISALLOW_NEW();

 public:
  LargestTextPaintManager(LocalFrameView*, PaintTimingDetector*);
  LargestTextPaintManager(const LargestTextPaintManager&) = delete;
  LargestTextPaintManager& operator=(const LargestTextPaintManager&) = delete;

  inline TextRecord* LargestText() {
    DCHECK(!largest_text_ || largest_text_->HasPaintTime());
    return largest_text_.Get();
  }
  void MaybeUpdateLargestText(TextRecord* record);
  void MaybeUpdateLargestIgnoredText(const LayoutObject&,
                                     const uint64_t&,
                                     const gfx::Rect& frame_visual_rect,
                                     const gfx::RectF& root_visual_rect);

  // Return the text LCP candidate and whether the candidate has changed.
  std::pair<TextRecord*, bool> UpdateMetricsCandidate();

  TextRecord* TakeLargestIgnoredText() {
    return std::exchange(largest_ignored_text_, nullptr);
  }
  const TextRecord* LargestIgnoredText() const { return largest_ignored_text_; }

  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;
  friend class TextPaintTimingDetectorTest;

  // The current largest text.
  Member<TextRecord> largest_text_;

  unsigned count_candidates_ = 0;

  // Text paints are ignored when they (or an ancestor) have opacity 0. This can
  // be a problem later on if the opacity changes to nonzero but this change is
  // composited. We solve this for the special case of documentElement by
  // storing a record for the largest ignored text without nested opacity. We
  // consider this an LCP candidate when the documentElement's opacity changes
  // from zero to nonzero.
  Member<TextRecord> largest_ignored_text_;

  Member<const LocalFrameView> frame_view_;
  Member<PaintTimingDetector> paint_timing_detector_;
};

// TextPaintTimingDetector contains Largest Text Paint and support for Text
// Element Timing.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within the viewport. Specifically, it:
// 1. Tracks all texts' first paints. If the text may be a largest text or is
// required by Element Timing, it records the visual size and paint time.
// 2. It keeps track of information regarding the largest text paint seen so
// far. Because the new version of LCP includes removed content, this record may
// only increase in size over time. See also this doc, which is now somewhat
// outdated: http://bit.ly/fcp_plus_plus.
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollected<TextPaintTimingDetector> {
  friend class TextPaintTimingDetectorTest;

 public:
  explicit TextPaintTimingDetector(LocalFrameView*, PaintTimingDetector*);
  TextPaintTimingDetector(const TextPaintTimingDetector&) = delete;
  TextPaintTimingDetector& operator=(const TextPaintTimingDetector&) = delete;

  bool ShouldWalkObject(const LayoutBoxModelObject&);
  void RecordAggregatedText(const LayoutBoxModelObject& aggregator,
                            const gfx::Rect& aggregated_visual_rect,
                            const PropertyTreeStateOrAlias&);
  std::optional<base::OnceCallback<void(const base::TimeTicks&,
                                        const DOMPaintTimingInfo&)>>
  TakePaintTimingCallback();
  void LayoutObjectWillBeDestroyed(const LayoutObject&);
  void StopRecordingLargestTextPaint();
  void ResetCallbackManager(PaintTimingCallbackManager* manager) {
    callback_manager_ = manager;
  }

  // Mark that the `LayoutObject` should be considered for paint timing, even if
  // it's already been painted, because it was modified as part of an
  // interaction (after hard LCP has stopped). This will not cause new element
  // timing entries to be emitted.
  void ResetPaintTrackingOnInteraction(const LayoutObject&);

  inline bool IsRecordingLargestTextPaint() const {
    return recording_largest_text_paint_;
  }
  inline std::pair<TextRecord*, bool> UpdateMetricsCandidate() {
    return ltp_manager_.UpdateMetricsCandidate();
  }
  void ReportLargestIgnoredText();
  void Trace(Visitor*) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;

  // The state of `LayoutObject`s being tracked in the `recorded_set_`.
  enum class TextPaintStatus { kPainted, kAllowRepaint };

  void AssignPaintTimeToQueuedRecords(uint32_t frame_index,
                                      const base::TimeTicks&,
                                      const DOMPaintTimingInfo&);
  TextRecord* MaybeRecordTextRecord(
      const LayoutObject& object,
      const uint64_t& visual_size,
      const PropertyTreeStateOrAlias& property_tree_state,
      const gfx::Rect& frame_visual_rect,
      const gfx::RectF& root_visual_rect,
      SoftNavigationContext* context,
      bool is_repaint);

  inline void QueueToMeasurePaintTime(const LayoutObject& object,
                                      TextRecord* record) {
    texts_queued_for_paint_time_.insert(&object, record);
    added_entry_in_latest_frame_ = true;
  }

  // LayoutObjects for which text has been aggregated.
  HeapHashMap<Member<const LayoutObject>, TextPaintStatus> recorded_set_;
  HeapHashSet<Member<const LayoutObject>> rewalkable_set_;

  // Text records queued for paint time. Indexed by LayoutObject to make removal
  // easy.
  HeapHashMap<Member<const LayoutObject>, Member<TextRecord>>
      texts_queued_for_paint_time_;

  Member<PaintTimingCallbackManager> callback_manager_;
  Member<const LocalFrameView> frame_view_;
  // Set lazily because we may not have the correct Window when first
  // initializing this class.
  Member<TextElementTiming> text_element_timing_;

  LargestTextPaintManager ltp_manager_;
  bool recording_largest_text_paint_ = true;

  // Used to decide which frame a record belongs to, monotonically increasing.
  uint32_t frame_index_ = 1;
  bool added_entry_in_latest_frame_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_PAINT_TIMING_DETECTOR_H_
