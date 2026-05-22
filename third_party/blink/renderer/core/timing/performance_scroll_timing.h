// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SCROLL_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SCROLL_TIMING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

class Node;

class CORE_EXPORT PerformanceScrollTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PerformanceScrollTiming(DOMHighResTimeStamp start_time,
                          DOMHighResTimeStamp duration,
                          DOMHighResTimeStamp first_frame_time,
                          int delta_x,
                          int delta_y,
                          const AtomicString& scroll_source,
                          unsigned frames_expected,
                          unsigned frames_produced,
                          DOMHighResTimeStamp checkerboard_time,
                          Node* target,
                          DOMWindow* source,
                          uint64_t navigation_id);

  ~PerformanceScrollTiming() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  Node* target() const;
  DOMHighResTimeStamp firstFrameTime() const { return first_frame_time_; }
  int deltaX() const { return delta_x_; }
  int deltaY() const { return delta_y_; }
  const AtomicString& scrollSource() const { return scroll_source_; }
  unsigned framesExpected() const { return frames_expected_; }
  unsigned framesProduced() const { return frames_produced_; }
  DOMHighResTimeStamp checkerboardTime() const { return checkerboard_time_; }

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  WeakMember<Node> target_;
  DOMHighResTimeStamp first_frame_time_;
  int delta_x_;
  int delta_y_;
  AtomicString scroll_source_;
  unsigned frames_expected_;
  unsigned frames_produced_;
  DOMHighResTimeStamp checkerboard_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SCROLL_TIMING_H_
