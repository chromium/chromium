// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_marker.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_sample.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_trace.h"
#include "v8/include/v8.h"
namespace blink {

TEST(ProfilerTraceBuilderTest, AddVMStateMarker) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, nullptr, base::TimeTicks::Now());

  base::TimeTicks sample_ticks = base::TimeTicks::Now();
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::GC);

  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 1u);
  auto* sample = samples.at(0).Get();
  EXPECT_EQ(sample->marker(), V8ProfilerMarker::Enum::kGc);
}

}  // namespace blink
