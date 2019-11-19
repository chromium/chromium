// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LAYOUT_SHIFT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LAYOUT_SHIFT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

// Exposes the layout shift score of an animation frame, computed as described
// in http://bit.ly/lsm-explainer.
class CORE_EXPORT LayoutShift final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LayoutShift(double start_time,
              double value,
              bool input_detected,
              double input_timestamp);
  ~LayoutShift() override;

  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  double value() const { return value_; }
  bool hadRecentInput() const { return had_recent_input_; }
  double lastInputTime() const { return most_recent_input_timestamp_; }

  void Trace(blink::Visitor*) override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  double value_;
  bool had_recent_input_;
  DOMHighResTimeStamp most_recent_input_timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LAYOUT_SHIFT_H_
