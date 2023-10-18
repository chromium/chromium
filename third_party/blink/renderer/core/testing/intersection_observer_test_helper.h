// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERSECTION_OBSERVER_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERSECTION_OBSERVER_TEST_HELPER_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_delegate.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"

namespace blink {

class TestIntersectionObserverDelegate : public IntersectionObserverDelegate {
 public:
  explicit TestIntersectionObserverDelegate(
      Document& document,
      LocalFrameUkmAggregator::MetricId metric_id =
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver)
      : document_(document), metric_id_(metric_id), call_count_(0) {}
  // TODO(szager): Add tests for the synchronous delivery code path. There is
  // already some indirect coverage by unit tests exercising features that rely
  // on it, but we should have some direct coverage in here.
  LocalFrameUkmAggregator::MetricId GetUkmMetricId() const override {
    return metric_id_;
  }
  IntersectionObserver::DeliveryBehavior GetDeliveryBehavior() const override {
    return IntersectionObserver::kPostTaskToDeliver;
  }
  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>& entries,
               IntersectionObserver&) override {
    call_count_++;
    entries_.AppendVector(entries);
  }
  bool NeedsInitialObservationWithDetachedTarget() const override {
    return true;
  }
  ExecutionContext* GetExecutionContext() const override {
    return document_->GetExecutionContext();
  }
  int CallCount() const { return call_count_; }
  int EntryCount() const { return entries_.size(); }
  const IntersectionObserverEntry* LastEntry() const {
    return entries_.back().Get();
  }
  void Clear() {
    entries_.clear();
    call_count_ = 0;
  }
  PhysicalRect LastIntersectionRect() const {
    if (entries_.empty())
      return PhysicalRect();
    const IntersectionGeometry& geometry = entries_.back()->GetGeometry();
    return geometry.IntersectionRect();
  }

  void Trace(Visitor* visitor) const override {
    IntersectionObserverDelegate::Trace(visitor);
    visitor->Trace(document_);
    visitor->Trace(entries_);
  }

 private:
  Member<Document> document_;
  LocalFrameUkmAggregator::MetricId metric_id_;
  HeapVector<Member<IntersectionObserverEntry>> entries_;
  int call_count_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERSECTION_OBSERVER_TEST_HELPER_H_
