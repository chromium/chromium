// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_MEASURE_CONDITIONAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_MEASURE_CONDITIONAL_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class DOMWindow;
class LocalDOMWindow;

// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/ConditionalTracing/explainer-for-loaf.md
class CORE_EXPORT PerformanceMeasureConditional final
    : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // `window` is the window where the measure is taken, and `source` is the
  // window where the measure is exposed.
  static PerformanceMeasureConditional* Create(const AtomicString& name,
                                               base::TimeTicks start_time,
                                               base::TimeTicks end_time,
                                               LocalDOMWindow* window,
                                               DOMWindow* source,
                                               uint64_t navigation_id);

  PerformanceMeasureConditional(const AtomicString& name,
                                double start_time,
                                double duration,
                                LocalDOMWindow* window,
                                DOMWindow* source,
                                uint64_t navigation_id);

  ~PerformanceMeasureConditional() override = default;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  // Return `window_` if it's not detached, and nullptr otherwise.
  // TODO(crbug.com/557288873): Add this behavior to the spec.
  DOMWindow* window() const;

  void Trace(Visitor*) const override;

 private:
  WeakMember<LocalDOMWindow> window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_MEASURE_CONDITIONAL_H_
