// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_RECORD_H_

#include <memory>
#include <optional>
#include <type_traits>

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/effective_visual_size_result.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/media_record_id.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
class MediaTiming;
class Node;
class SoftNavigationContext;
class TracedValue;

class CORE_EXPORT PaintTimingRecord
    : public GarbageCollected<PaintTimingRecord> {
 public:
  PaintTimingRecord(Node*,
                    const gfx::Rect& frame_visual_rect,
                    const gfx::RectF& root_visual_rect);

  PaintTimingRecord(const PaintTimingRecord&) = delete;
  PaintTimingRecord& operator=(const PaintTimingRecord&) = delete;

  virtual void Trace(Visitor*) const;
  virtual void PopulateTraceValue(TracedValue&) const;
  virtual bool IsImageRecord() const { return false; }
  virtual bool IsTextRecord() const { return false; }

  // Returns the "effective visual size" of the record, which is defined in
  // https://w3c.github.io/largest-contentful-paint/#effective-visual-size.
  virtual uint64_t EffectiveVisualSize() const = 0;

  Node* GetNode() const { return node_.Get(); }
  int NodeIdForTracing() const;

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

  SoftNavigationContext* GetSoftNavigationContext() const {
    return soft_navigation_context_;
  }
  void SetSoftNavigationContext(SoftNavigationContext* context) {
    soft_navigation_context_ = context;
  }
  // Returns true iff this record is needed to compute Interaction Contentful
  // Paint (ICP).
  bool IsNeededForInteractionContentfulPaint() const {
    return !!soft_navigation_context_;
  }

  // Returns true iff this record is needed to compute Largest Contentful Paint
  // (LCP) for hard navigations.
  bool IsNeededForLargestContentfulPaint() const { return is_needed_for_lcp_; }
  void SetIsNeededForLargestContentfulPaint(bool value) {
    is_needed_for_lcp_ = value;
  }

  // Returns whether or not the corresponding image or text was removed from the
  // DOM after the record was created. Used to ensure we get paint timing for
  // such records without reporting them as LCP candidates.
  bool WasNodeRemoved() const;

  // Returns true if this record's effective size is larger than `other`'s
  // effective size (null records are considered to have no size) and false
  // otherwise. See also
  // https://www.w3.org/TR/largest-contentful-paint/#sec-effective-visual-size.
  bool IsEffectiveSizeLargerThan(PaintTimingRecord* other) const {
    return EffectiveVisualSize() > (other ? other->EffectiveVisualSize() : 0u);
  }

 private:
  const WeakMember<Node> node_;
  const WeakMember<LayoutObject> layout_object_;
  const gfx::RectF root_visual_rect_;
  bool is_needed_for_lcp_ = false;
  base::TimeTicks paint_time_;
  DOMPaintTimingInfo paint_timing_info_;
  Member<SoftNavigationContext> soft_navigation_context_;
  // LCP rect information, only populated when tracing is enabled.
  std::optional<LCPRectInfo> lcp_rect_info_;
};

class CORE_EXPORT TextRecord final : public PaintTimingRecord {
 public:
  TextRecord(Node* node,
             uint64_t new_recorded_size,
             const gfx::RectF& element_timing_rect,
             const gfx::Rect& frame_visual_rect,
             const gfx::RectF& root_visual_rect);

  bool IsTextRecord() const override { return true; }
  uint64_t EffectiveVisualSize() const override {
    return effective_visual_size_;
  }

  uint32_t FrameIndex() const { return frame_index_; }
  void SetFrameIndex(uint32_t index) { frame_index_ = index; }

  bool IsNeededForElementTiming() const {
    return is_needed_for_element_timing_;
  }
  void SetIsNeededForElementTiming(bool value) {
    is_needed_for_element_timing_ = value;
  }
  const gfx::RectF& ElementTimingRect() const { return element_timing_rect_; }

 private:
  uint32_t frame_index_ = 0;
  const uint64_t effective_visual_size_;
  const gfx::RectF element_timing_rect_;
  bool is_needed_for_element_timing_ = false;
};

// TODO(yoav): Rename all mentions of "image" to "media"
class CORE_EXPORT ImageRecord final : public PaintTimingRecord {
 public:
  ImageRecord(Node*,
              const MediaTiming*,
              const gfx::Rect& frame_visual_rect,
              const gfx::RectF& root_visual_rect,
              MediaRecordIdHash,
              const EffectiveVisualSizeResult&);

  void PopulateTraceValue(TracedValue&) const override;
  void Trace(Visitor* visitor) const override;
  bool IsImageRecord() const override { return true; }
  uint64_t EffectiveVisualSize() const override {
    return effective_visual_size_result_.size;
  }

  // Returns the image's entropy, in encoded-bits-per-layout-pixel, as used to
  // determine whether the image is a potential LCP candidate.
  double EntropyForLCP() const { return effective_visual_size_result_.entropy; }

  // Returns the image's loading priority. Will return `std::nullopt` if there
  // is no `media_timing`.
  std::optional<WebURLRequest::Priority> RequestPriority() const;

  // Returns or sets whether the image is sufficiently loaded to be considered
  // for reporting. This is set for all media based on the `media_timing_`'s
  // IsSufficientContentLoadedForPaint(), except for animated images with
  // ReportFirstFrameTimeAsRenderTime enabled, in which case it's based on the
  // `media_timing_`'s IsPaintedFirstFrame().
  bool IsSufficientlyLoadedForReporting() const {
    return is_sufficiently_loaded_for_reporting_;
  }
  void SetIsSufficientlyLoadedForReporting() {
    is_sufficiently_loaded_for_reporting_ = true;
  }

  // Returns or sets the load time of the image. Note that in some cases there
  // will not be a load time even when `IsSufficientlyLoadedForReporting()` is
  // true, e.g. first video frame and when using first animated frame for
  // images.
  base::TimeTicks LoadTime() const { return load_time_; }
  void SetLoadTime(base::TimeTicks value) { load_time_ = value; }

  // Returns or sets the first animated frame time. This is set for the first
  // video or animated image frame, and it's used for metrics (independently of
  // the `PaintTimingRecord`).
  base::TimeTicks FirstAnimatedFrameTime() const {
    return first_animated_frame_time_;
  }
  void SetFirstAnimatedFrameTime(base::TimeTicks value) {
    first_animated_frame_time_ = value;
  }
  bool HasFirstAnimatedFrameTime() const {
    return !first_animated_frame_time_.is_null();
  }

  MediaRecordIdHash Hash() const { return hash_; }
  const MediaTiming* GetMediaTiming() const { return media_timing_; }

  const EffectiveVisualSizeResult& GetEffectiveVisualSizeResult() const {
    return effective_visual_size_result_;
  }

 private:
  const WeakMember<const MediaTiming> media_timing_;
  const MediaRecordIdHash hash_;
  base::TimeTicks load_time_;
  base::TimeTicks first_animated_frame_time_;
  bool is_sufficiently_loaded_for_reporting_ = false;
  const EffectiveVisualSizeResult effective_visual_size_result_;
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

// Concept for generic algorithms that act on a collection of
// `PaintTimingRecord`s.
template <typename T>
concept IsDerivedFromPaintTimingRecord =
    std::derived_from<T, PaintTimingRecord>;

static_assert(std::is_trivially_destructible_v<TextRecord>,
              "Require trivial destruction for faster sweeping");
static_assert(std::is_trivially_destructible_v<ImageRecord>,
              "Require trivial destruction for faster sweeping");

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_RECORD_H_
