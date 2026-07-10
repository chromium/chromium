// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_MARK_CONDITIONAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_MARK_CONDITIONAL_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/ConditionalTracing/explainer-for-loaf.md
class CORE_EXPORT PerformanceMarkConditional final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PerformanceMarkConditional(const AtomicString& name,
                             base::TimeTicks start_time,
                             DOMWindow* source,
                             uint64_t navigation_id);

  ~PerformanceMarkConditional() override = default;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_MARK_CONDITIONAL_H_
