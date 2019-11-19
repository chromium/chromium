// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PROFILER_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PROFILER_GROUP_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class Profiler;
class ProfilerInitOptions;
class ScriptPromiseResolver;
class ScriptState;

// A ProfilerGroup represents a set of profilers sharing an underlying
// v8::CpuProfiler attached to a common isolate.
class CORE_EXPORT ProfilerGroup
    : public V8PerIsolateData::GarbageCollectedData {
 public:
  static ProfilerGroup* From(v8::Isolate*);

  static base::TimeDelta GetBaseSampleInterval();

  ProfilerGroup(v8::Isolate* isolate);
  ~ProfilerGroup() override;

  Profiler* CreateProfiler(ScriptState* script_state,
                           const ProfilerInitOptions&,
                           base::TimeTicks time_origin,
                           ExceptionState&);

  void WillBeDestroyed() override;
  void Trace(blink::Visitor*) override;

 private:
  friend class Profiler;

  void InitV8Profiler();
  void TeardownV8Profiler();

  void StopProfiler(ScriptState*, Profiler*, ScriptPromiseResolver*);

  // Cancels a profiler, discarding its associated trace.
  void CancelProfiler(Profiler*);

  // Generates an unused string identifier to use for a new profiling session.
  String NextProfilerId();

  v8::Isolate* const isolate_;
  v8::CpuProfiler* cpu_profiler_;
  int next_profiler_id_;
  int num_active_profilers_;

  HeapHashSet<WeakMember<Profiler>> profilers_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerGroup);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PROFILER_GROUP_H_
