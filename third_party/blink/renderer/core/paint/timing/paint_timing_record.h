// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_RECORD_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
class LCPRectInfo;
class MediaTiming;
class Node;
class SoftNavigationContext;
class TracedValue;

class CORE_EXPORT PaintTimingRecord
    : public GarbageCollected<PaintTimingRecord> {
 public:
  PaintTimingRecord(Node*,
                    uint64_t recorded_size,
                    const gfx::Rect& frame_visual_rect,
                    const gfx::RectF& root_visual_rect,
                    SoftNavigationContext*);

  PaintTimingRecord(const PaintTimingRecord&) = delete;
  PaintTimingRecord& operator=(const PaintTimingRecord&) = delete;

  virtual void Trace(Visitor*) const;
  virtual void PopulateTraceValue(TracedValue&) const;
  virtual bool IsImageRecord() const { return false; }
  virtual bool IsTextRecord() const { return false; }

  Node* GetNode() const { return node_.Get(); }
  int NodeIdForTracing() const;

  uint64_t RecordedSize() const { return recorded_size_; }
  const gfx::RectF& RootVisualRect() const { return root_visual_rect_; }

  bool HasPaintTime() const { return !paint_time_.is_null(); }
  base::TimeTicks PaintTime() const { return paint_time_; }
  void SetPaintTime(base::TimeTicks paint_time,
                    const DOMPaintTimingInfo& info) {
    paint_time_ = paint_time;
    paint_timing_info_ = info;
  }
  const DOMPaintTimingInfo& PaintTimingInfo() const {
    return paint_timing_info_;
  }

  uint32_t FrameIndex() const { return frame_index_; }
  void SetFrameIndex(uint32_t index) { frame_index_ = index; }

  SoftNavigationContext* GetSoftNavigationContext() const {
    return soft_navigation_context_;
  }
  void SetSoftNavigationContext(SoftNavigationContext* context) {
    soft_navigation_context_ = context;
  }

  // Returns whether or not the corresponding image or text was removed from the
  // DOM after the record was created and before getting paint timing. Used to
  // ensure we get paint timing for such records without reporting them as LCP
  // candidates.
  bool WasImageOrTextRemovedWhilePending() const {
    return was_image_or_text_removed_while_pending_;
  }
  void OnImageOrTextRemovedWhilePending() {
    was_image_or_text_removed_while_pending_ = true;
  }

 private:
  const WeakMember<Node> node_;
  const uint64_t recorded_size_;
  const gfx::RectF root_visual_rect_;
  uint32_t frame_index_ = 0;
  bool was_image_or_text_removed_while_pending_ = false;
  base::TimeTicks paint_time_;
  DOMPaintTimingInfo paint_timing_info_;
  Member<SoftNavigationContext> soft_navigation_context_;
  // LCP rect information, only populated when tracing is enabled.
  std::unique_ptr<LCPRectInfo> lcp_rect_info_;
};

class CORE_EXPORT TextRecord final : public PaintTimingRecord {
 public:
  TextRecord(Node* node,
             uint64_t new_recorded_size,
             const gfx::RectF& element_timing_rect,
             const gfx::Rect& frame_visual_rect,
             const gfx::RectF& root_visual_rect,
             bool is_needed_for_element_timing,
             SoftNavigationContext* soft_navigation_context);

  bool IsTextRecord() const override { return true; }

  const gfx::RectF& ElementTimingRect() const { return element_timing_rect_; }
  bool IsNeededForElementTiming() const {
    return is_needed_for_element_timing_;
  }

 private:
  const gfx::RectF element_timing_rect_;
  const bool is_needed_for_element_timing_;
};

// TODO(yoav): Rename all mentions of "image" to "media"
class CORE_EXPORT ImageRecord final : public PaintTimingRecord {
 public:
  ImageRecord(Node* node,
              const MediaTiming* new_media_timing,
              uint64_t new_recorded_size,
              const gfx::Rect& frame_visual_rect,
              const gfx::RectF& root_visual_rect,
              MediaRecordIdHash hash,
              double entropy_for_lcp,
              SoftNavigationContext* soft_navigation_context);

  void PopulateTraceValue(TracedValue&) const override;
  void Trace(Visitor* visitor) const override;
  bool IsImageRecord() const override { return true; }

  // Returns the image's entropy, in encoded-bits-per-layout-pixel, as used to
  // determine whether the image is a potential LCP candidate.
  double EntropyForLCP() const { return entropy_for_lcp_; }

  // Returns the image's loading priority. Will return `std::nullopt` if there
  // is no `media_timing`.
  std::optional<WebURLRequest::Priority> RequestPriority() const;

  bool IsLoaded() const { return is_loaded_; }
  void MarkLoaded() { is_loaded_ = true; }

  bool HasLoadTime() const { return !load_time_.is_null(); }
  base::TimeTicks LoadTime() const { return load_time_; }
  void SetLoadTime(base::TimeTicks value) { load_time_ = value; }

  bool HasFirstAnimatedFrameTime() const {
    return !first_animated_frame_time_.is_null();
  }
  base::TimeTicks FirstAnimatedFrameTime() const {
    return first_animated_frame_time_;
  }
  void SetFirstAnimatedFrameTime(base::TimeTicks value) {
    first_animated_frame_time_ = value;
  }

  bool IsFirstAnimatedFramePaintTimingQueued() {
    return is_first_animated_frame_paint_timing_queued_;
  }
  void SetIsFirstAnimatedFramePaintTimingQueued(bool value) {
    is_first_animated_frame_paint_timing_queued_ = value;
  }

  MediaRecordIdHash Hash() const { return hash_; }
  const MediaTiming* GetMediaTiming() const { return media_timing_; }

 private:
  const WeakMember<const MediaTiming> media_timing_;
  const MediaRecordIdHash hash_;
  base::TimeTicks load_time_;
  base::TimeTicks first_animated_frame_time_;
  bool is_first_animated_frame_paint_timing_queued_ = false;
  bool is_loaded_ = false;
  const double entropy_for_lcp_;
};

template <>
struct DowncastTraits<TextRecord> {
  static bool AllowFrom(const PaintTimingRecord& record) {
    return record.IsTextRecord();
  }
};

template <>
struct DowncastTraits<ImageRecord> {
  static bool AllowFrom(const PaintTimingRecord& record) {
    return record.IsImageRecord();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_RECORD_H_
