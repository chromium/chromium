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
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/navigation_id_generator.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
  static uint64_t last_context_id_;

 public:
  // Each `SoftNavigationContext` has a strictly increasing numeric ID
  // (`ContextId()`), which that can be used to determine the order of
  // interactions. `NextContextId()` is the next ID that will be used, and it
  // can be used to determine order with respect to a certain point, e.g. to
  // differentiate new interactions from previous ones.
  static uint64_t NextContextId() { return last_context_id_ + 1; }

  SoftNavigationContext(LocalDOMWindow& window,
                        features::SoftNavigationHeuristicsMode);

  // LargestContentfulPaintCalculator::Delegate:
  void EmitPerformanceEntry(const DOMPaintTimingInfo& paint_timing_info,
                            uint64_t paint_size,
                            base::TimeTicks load_time,
                            const AtomicString& id,
                            const String& url,
                            Element* element) override;
  bool IsHardNavigation() const override { return false; }
  void Trace(Visitor* visitor) const override;

  bool IsMostRecentlyCreatedContext() const {
    return context_id_ == last_context_id_;
  }

  bool HasNavigationId() const {
    return navigation_id_ != kNavigationIdAbsentValue;
  }
  uint32_t NavigationId() const { return navigation_id_; }
  void SetNavigationId(uint32_t navigation_id) {
    navigation_id_ = navigation_id;
  }

  base::TimeTicks UserInteractionTimestamp() const {
    return user_interaction_timestamp_;
  }
  void SetUserInteractionTimestamp(base::TimeTicks value) {
    user_interaction_timestamp_ = value;
  }

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

  // First Url and Last Url help for cases with multiple client-side redirects.
  const String& InitialUrl() const { return initial_url_; }
  void AddUrl(const String& url,
              base::UnguessableToken same_document_metrics_token) {
    if (initial_url_.empty()) {
      initial_url_ = url;
      same_document_metrics_token_ = same_document_metrics_token;
    }
    most_recent_url_ = url;
  }
  bool HasUrl() const { return !initial_url_.empty(); }
  base::UnguessableToken SameDocumentMetricsToken() const {
    return same_document_metrics_token_;
  }

  void AddModifiedNode(Node* node);
  // Returns true if this paint updated the attributed area, and so we should
  // check for sufficient paints to emit a soft-nav entry.
  bool HasDomModification() const { return num_modified_dom_nodes_ > 0; }

  uint64_t PaintedArea() const { return painted_area_; }
  uint64_t ContextId() const { return context_id_; }

  // Returns true if this Context is involved in modifying the container root
  // for this Node*.
  bool IsNeededForTiming(Node* node);
  // Reports a new contentful paint area to this context, and the Node painted.
  bool AddPaintedArea(PaintTimingRecord*);
  // Returns true if we update the total attributed area this animation frame.
  // Used to check if it is worthwhile to call `SatisfiesSoftNavPaintCriteria`.
  bool OnPaintFinished();
  void OnInputOrScroll();
  bool TryUpdateLcpCandidate();
  void UpdateWebExposedLargestContentfulPaintIfNeeded();
  const LargestContentfulPaintDetails& LatestLcpDetailsForUkm();

  bool SatisfiesSoftNavNonPaintCriteria() const;
  bool SatisfiesSoftNavPaintCriteria(uint64_t required_paint_area) const;

  bool IsRecordingLargestContentfulPaint() const {
    return first_input_or_scroll_time_.is_null();
  }

  bool WasEmitted() const { return was_emitted_; }
  void MarkEmitted() { was_emitted_ = true; }

  void WriteIntoTrace(perfetto::TracedValue context) const;

  // Called when `SoftNavigationHeuristics` is shut down on frame detach.
  void Shutdown();

 private:
  // Pre-Increment `last_context_id_` such that the newest context uses the
  // largest value and can be used to identify the most recent context.
  const uint64_t context_id_ = ++last_context_id_;

  uint32_t navigation_id_ = kNavigationIdAbsentValue;
  const features::SoftNavigationHeuristicsMode paint_attribution_mode_;
  bool was_emitted_ = false;

  base::TimeTicks user_interaction_timestamp_;
  base::TimeTicks first_input_or_scroll_time_;

  String initial_url_;
  base::UnguessableToken same_document_metrics_token_;
  String most_recent_url_;

  blink::HeapHashSet<WeakMember<Node>> modified_nodes_;
  blink::HeapHashSet<WeakMember<Node>> already_painted_modified_nodes_;

  Member<LocalDOMWindow> window_;
  Member<LargestContentfulPaintCalculator> lcp_calculator_;
  Member<TextRecord> largest_text_;
  Member<ImageRecord> largest_image_;
  Member<PaintTimingRecord> first_image_or_text_;

  // Elements of `modified_nodes_` can get GC-ed, so we need to keep a count of
  // the total nodes modified.
  size_t num_modified_dom_nodes_ = 0;
  uint64_t painted_area_ = 0;
  uint64_t repainted_area_ = 0;

  size_t num_modified_dom_nodes_last_animation_frame_ = 0;
  size_t num_live_nodes_last_animation_frame_ = 0;
  uint64_t painted_area_last_animation_frame_ = 0;
  uint64_t repainted_area_last_animation_frame_ = 0;

  WeakMember<Node> known_not_related_parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_CONTEXT_H_
