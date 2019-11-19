// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/internals_profiler.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

#include "v8/include/v8-profiler.h"

namespace blink {

void InternalsProfiler::collectSample(ScriptState* script_state, Internals&) {
  v8::CpuProfiler::CollectSample(script_state->GetIsolate());
}

}  // namespace blink
