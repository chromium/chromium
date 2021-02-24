// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

class CORE_EXPORT PerformanceEventTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PerformanceEventTiming* Create(const AtomicString& event_type,
                                        DOMHighResTimeStamp start_time,
                                        DOMHighResTimeStamp processing_start,
                                        DOMHighResTimeStamp processing_end,
                                        bool cancelable,
                                        Node* target);

  static PerformanceEventTiming* CreateFirstInputTiming(
      PerformanceEventTiming* entry);

  PerformanceEventTiming(const AtomicString& event_type,
                         const AtomicString& entry_type,
                         DOMHighResTimeStamp start_time,
                         DOMHighResTimeStamp processing_start,
                         DOMHighResTimeStamp processing_end,
                         bool cancelable,
                         Node* target);
  ~PerformanceEventTiming() override;

  AtomicString entryType() const override { return entry_type_; }
  PerformanceEntryType EntryTypeEnum() const override;

  bool cancelable() const { return cancelable_; }

  DOMHighResTimeStamp processingStart() const;
  DOMHighResTimeStamp processingEnd() const;

  Node* target() const;

  void SetDuration(double duration);

  void BuildJSONValue(V8ObjectBuilder&) const override;

  void Trace(Visitor*) const override;

  std::unique_ptr<TracedValue> ToTracedValue() const;

 private:
  AtomicString entry_type_;
  DOMHighResTimeStamp processing_start_;
  DOMHighResTimeStamp processing_end_;
  bool cancelable_;
  WeakMember<Node> target_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_
