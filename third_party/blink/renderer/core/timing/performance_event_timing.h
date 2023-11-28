// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

class Frame;

class CORE_EXPORT PerformanceEventTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PerformanceEventTiming* Create(const AtomicString& event_type,
                                        DOMHighResTimeStamp start_time,
                                        DOMHighResTimeStamp processing_start,
                                        DOMHighResTimeStamp processing_end,
                                        bool cancelable,
                                        Node* target,
                                        DOMWindow* source);

  static PerformanceEventTiming* CreateFirstInputTiming(
      PerformanceEventTiming* entry);

  PerformanceEventTiming(const AtomicString& event_type,
                         const AtomicString& entry_type,
                         DOMHighResTimeStamp start_time,
                         DOMHighResTimeStamp processing_start,
                         DOMHighResTimeStamp processing_end,
                         bool cancelable,
                         Node* target,
                         DOMWindow* source);
  ~PerformanceEventTiming() override;

  const AtomicString& entryType() const override { return entry_type_; }
  PerformanceEntryType EntryTypeEnum() const override;

  bool cancelable() const { return cancelable_; }

  DOMHighResTimeStamp processingStart() const;
  DOMHighResTimeStamp processingEnd() const;

  Node* target() const;

  uint32_t interactionId() const;

  uint32_t interactionOffset() const;

  void SetInteractionIdAndOffset(uint32_t interaction_id,
                                 uint32_t interaction_offset);

  base::TimeTicks unsafePresentationTimestamp() const;

  void SetUnsafePresentationTimestamp(base::TimeTicks presentation_timestamp);

  void SetDuration(double duration);

  void BuildJSONValue(V8ObjectBuilder&) const override;

  void Trace(Visitor*) const override;

  std::unique_ptr<TracedValue> ToTracedValue(Frame* frame) const;

 private:
  AtomicString entry_type_;
  DOMHighResTimeStamp processing_start_;
  DOMHighResTimeStamp processing_end_;
  bool cancelable_;
  WeakMember<Node> target_;
  uint32_t interaction_id_ = 0;
  uint32_t interaction_offset_ = 0;

  // This is the exact (non-rounded) monotonic timestamp for presentation, which
  // is currently only used by eventTiming trace events to report accurate
  // ending time. It should not be exposed to performance observer API entries
  // for security and privacy reasons.
  base::TimeTicks unsafe_presentation_timestamp_ = base::TimeTicks::Min();
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_EVENT_TIMING_H_
