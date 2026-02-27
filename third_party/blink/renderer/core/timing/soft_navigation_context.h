// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_

#include <cstdint>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

struct LargestContentfulPaintDetails;
class LocalDOMWindow;
class Node;
class SoftNavigationHeuristics;

class CORE_EXPORT SoftNavigationContext
    : public GarbageCollected<SoftNavigationContext>,
      public LargestContentfulPaintCalculator::Delegate {
  USING_PRE_FINALIZER(SoftNavigationContext, Dispose);

 public:
  // Each `SoftNavigationContext` has a strictly increasing numeric ID
  // (`ContextId()`), which that can be used to determine the order of
  // interactions. `NextContextId()` is the next ID that will be used, and it
  // can be used to determine order with respect to a certain point, e.g. to
  // differentiate new interactions from previous ones.
  static uint64_t NextContextId() { return last_context_id_ + 1; }

  explicit SoftNavigationContext(LocalDOMWindow& window);

  // LargestContentfulPaintCalculator::Delegate:
  void EmitLcpPerformanceEntry(const DOMPaintTimingInfo& paint_timing_info,
                               uint64_t paint_size,
                               base::TimeTicks load_time,
                               const AtomicString& id,
                               const String& url,
                               Element* element) override;
  void OnLcpMetricsForReportingChanged() override;
  bool IsHardNavigation() const override { return false; }
  void Trace(Visitor* visitor) const override;

  bool IsMostRecentlyCreatedContext() const {
    return context_id_ == last_context_id_;
  }

  bool HasNavigationId() const {
    return navigation_id_ != PerformanceTimelineEntryIdInfo::kNoId;
  }
  uint64_t NavigationId() const { return navigation_id_; }
  void SetNavigationId(uint64_t navigation_id) {
    navigation_id_ = navigation_id;
  }

  // The time origin is used for calculating soft navigation timings, especially
  // Soft LCP. It is the earlier of the processing end of the interaction and
  // the url change time. Once the processing of the interaction has ended, it
  // is guaranteed to be available; once the URL change time has been set,
  // it's available and final. When not available it may be null.
  base::TimeTicks TimeOrigin() const;

  // Indicates when the interaction processing is finished, which is after the
  // event handler has finished executing.
  base::TimeTicks ProcessingEnd() const { return processing_end_; }
  void SetProcessingEnd(base::TimeTicks value) { processing_end_ = value; }

  // This is set to the time of the first URL change and is not updated
  // afterwards.
  base::TimeTicks UrlChangeTime() const { return url_change_time_; }

  // Sets URL, retaining the first URL only and the first
  // |same_document_metrics_token|, while also recording the UrlChangeTime() as
  // base::TimeTicks::Now().
  void AddUrl(const String& url,
              V8NavigationType::Enum navigation_type,
              base::UnguessableToken same_document_metrics_token);

  // Returns the type of the initial same document navigation (first call to
  // `AddUrl()`). Must not be called before the URL is set.
  V8NavigationType::Enum NavigationType() const {
    CHECK(HasUrl());
    return navigation_type_;
  }

  base::UnguessableToken SameDocumentMetricsToken() const {
    return same_document_metrics_token_;
  }

  // A single interaction / navigation may change URLs multiple times.
  // For now, we use the initial URL value as the URL to attribute the
  // performance data to-- but it is reasonable to evaluate using the final URL
  // as an alternative.
  const String& AttributionUrl() const { return initial_url_; }
  bool HasUrl() const { return !initial_url_.empty(); }

  bool HasFirstContentfulPaint() const {
    return first_image_or_text_ && first_image_or_text_->HasPaintTime();
  }
  base::TimeTicks FirstContentfulPaint() const {
    return first_image_or_text_ ? first_image_or_text_->PaintTime()
                                : base::TimeTicks();
  }
  const DOMPaintTimingInfo& FirstContentfulPaintTimingInfo() const {
    CHECK(HasFirstContentfulPaint());
    return first_image_or_text_->PaintTimingInfo();
  }

  void AddModifiedNode(Node* node);
  // Returns true if this paint updated the attributed area, and so we should
  // check for sufficient paints to emit a soft-nav entry.
  bool HasDomModification() const { return num_modified_dom_nodes_ > 0; }

  uint64_t PaintedArea() const { return painted_area_; }
  uint64_t ContextId() const { return context_id_; }

  // Reports a new contentful paint area to this context, and the Node painted.
  bool AddPaintedArea(PaintTimingRecord*);
  // Returns true if we update the total attributed area this animation frame.
  // Used to check if it is worthwhile to call `SatisfiesSoftNavPaintCriteria`.
  bool OnPaintFinished();
  void OnInputOrScroll();
  void TryUpdateLcpCandidate();
  const LargestContentfulPaintDetails& LatestLcpDetailsForUkm();

  bool SatisfiesSoftNavNonPaintCriteria() const;
  bool SatisfiesSoftNavPaintCriteria(uint64_t required_paint_area) const;

  bool IsRecordingLargestContentfulPaint() const {
    return first_input_or_scroll_time_.is_null();
  }

  // Emits the soft navigation performance entry. The context must not have been
  // previously emitted. `WasEmitted()` returns true after this is called.
  //
  // Note: There are several reasons why we might have an FCP but have not
  // emitted an ICP, all of which should be fixed:
  //   1. crbug.com/383568320: For <video>, we set the paint timestamp for the
  //      first video frame outside of paint, but require BeginMainFrame +
  //      presentation feedback to emit the ICP entry. The soft nav entry can be
  //      emitted in this gap.
  //  2. crbug.com/454082773: If the FCP element is detached from the DOM before
  //     its presentation feedback is processed, we won't emit an ICP entry for
  //     this.
  //  3. crbug.com/454082771, crbug.com/434160944: We overwrite image and text
  //     candidates during paint, which affects which candidates we emit when
  //     processing presentation feedback. For example, if we paint a text node
  //     (FCP) in frame 1 and a larger image in frame 2, and the feedback for
  //     frame 1 arrives after frame 2, the image blocks emitting the ICP entry
  //     for the text. Moving more logic into presentation time, like we do for
  //     hard LCP, in conjunction with emitting largest presented image/text
  //     (vs. pending image) would fix this.
  void EmitSoftNavigation();
  bool WasEmitted() const { return was_emitted_; }

  void WriteIntoTrace(perfetto::TracedValue context) const;

  // Called when `SoftNavigationHeuristics` is shut down on frame detach.
  void Shutdown();

  void Dispose();

 private:
  static uint64_t last_context_id_;

  // largest value and can be used to identify the most recent context.
  const uint64_t context_id_ = ++last_context_id_;

  uint64_t navigation_id_ = PerformanceTimelineEntryIdInfo::kNoId;
  bool was_emitted_ = false;

  base::TimeTicks first_input_or_scroll_time_;
  base::TimeTicks url_change_time_;
  base::TimeTicks processing_end_;

  String initial_url_;
  base::UnguessableToken same_document_metrics_token_;
  V8NavigationType::Enum navigation_type_ = V8NavigationType::Enum::kPush;

  Member<LocalDOMWindow> window_;
  Member<LargestContentfulPaintCalculator> lcp_calculator_;
  Member<PaintTimingRecord> first_image_or_text_;

  size_t num_modified_dom_nodes_ = 0;
  uint64_t painted_area_ = 0;

  size_t num_modified_dom_nodes_last_animation_frame_ = 0;
  uint64_t painted_area_last_animation_frame_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_
