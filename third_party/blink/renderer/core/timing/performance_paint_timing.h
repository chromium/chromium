// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_PAINT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_PAINT_TIMING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

class CORE_EXPORT PerformancePaintTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class PaintType { kFirstPaint, kFirstContentfulPaint };

  PerformancePaintTiming(PaintType,
                         DOMHighResTimeStamp start_time,
                         DOMHighResTimeStamp rendering_update_end_time,
                         DOMWindow* source,
                         bool is_triggered_by_soft_navigation);
  ~PerformancePaintTiming() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;
  DOMHighResTimeStamp paintTime() const { return rendering_update_end_time_; }
  DOMHighResTimeStamp presentationTime() const { return startTime(); }

 protected:
  void BuildJSONValue(V8ObjectBuilder&) const override;

 private:
  DOMHighResTimeStamp rendering_update_end_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_PAINT_TIMING_H_
