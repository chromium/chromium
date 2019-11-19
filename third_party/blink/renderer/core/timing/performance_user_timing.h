/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_USER_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_USER_TIMING_H_

#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ExceptionState;
class Performance;

using PerformanceEntryMap = HeapHashMap<AtomicString, PerformanceEntryVector>;

class UserTiming final : public GarbageCollected<UserTiming> {
 public:
  explicit UserTiming(Performance&);

  PerformanceMark* CreatePerformanceMark(ScriptState*,
                                         const AtomicString& mark_name,
                                         PerformanceMarkOptions*,
                                         ExceptionState&);

  void ClearMarks(const AtomicString& mark_name);

  PerformanceMeasure* Measure(ScriptState*,
                              const AtomicString& measure_name,
                              const StringOrDouble& start,
                              base::Optional<double> duration,
                              const StringOrDouble& end,
                              const ScriptValue& detail,
                              ExceptionState&);
  void ClearMeasures(const AtomicString& measure_name);

  PerformanceEntryVector GetMarks() const;
  PerformanceEntryVector GetMeasures() const;
  void AddMarkToPerformanceTimeline(PerformanceMark&);

  PerformanceEntryVector GetMarks(const AtomicString& name) const;
  PerformanceEntryVector GetMeasures(const AtomicString& name) const;

  void Trace(blink::Visitor*);

 private:
  double FindExistingMarkStartTime(const AtomicString& mark_name,
                                   ExceptionState&);
  double GetTimeOrFindMarkTime(const AtomicString& measure_name,
                               const StringOrDouble& mark_or_time,
                               ExceptionState&);

  Member<Performance> performance_;
  PerformanceEntryMap marks_map_;
  PerformanceEntryMap measures_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_USER_TIMING_H_
