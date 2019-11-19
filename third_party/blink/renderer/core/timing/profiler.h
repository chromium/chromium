// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PROFILER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PROFILER_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/profiler_group.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptState;

// A web-exposed JS sampling profiler created via blink::ProfilerGroup,
// wrapping a handle to v8::CpuProfiler. Records samples periodically from the
// isolate until stopped.
class CORE_EXPORT Profiler final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Profiler(ProfilerGroup* profiler_group,
           const String& profiler_id,
           int target_sample_rate,
           scoped_refptr<const SecurityOrigin> source_origin,
           base::TimeTicks time_origin)
      : profiler_group_(profiler_group),
        profiler_id_(profiler_id),
        target_sample_rate_(target_sample_rate),
        source_origin_(source_origin),
        time_origin_(time_origin) {}

  ~Profiler() override;

  void Trace(blink::Visitor* visitor) override;

  void Dispose();

  String ProfilerId() const { return profiler_id_; }
  int TargetSampleRate() const { return target_sample_rate_; }
  const SecurityOrigin* SourceOrigin() const { return source_origin_.get(); }
  base::TimeTicks TimeOrigin() const { return time_origin_; }

  DOMHighResTimeStamp sampleInterval() { return target_sample_rate_; }
  bool stopped() const { return !profiler_group_; }
  ScriptPromise stop(ScriptState*);

 private:
  Member<ProfilerGroup> profiler_group_;
  const String profiler_id_;
  const int target_sample_rate_;
  const scoped_refptr<const SecurityOrigin> source_origin_;
  const base::TimeTicks time_origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PROFILER_H_
