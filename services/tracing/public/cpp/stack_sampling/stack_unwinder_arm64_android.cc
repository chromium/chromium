// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/stack_unwinder_arm64_android.h"

#include "base/debug/stack_trace.h"

namespace tracing {

bool UnwinderArm64::CanUnwindFrom(const base::Frame& current_frame) const {
  return true;
}

base::UnwindResult UnwinderArm64::TryUnwind(
    base::RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<base::Frame>* stack) const {
  uintptr_t fp = thread_context->regs[29];
  constexpr size_t kMaxDepth = 40;
  const void* out_trace[kMaxDepth] = {};
  // If the fp is not valid, then pass the stack pointer as fp. The unwind
  // function scans the stack to find the next frame.
  if (fp < thread_context->sp || fp >= stack_top) {
    fp = thread_context->sp;
  }
  size_t depth = base::debug::TraceStackFramePointersFromBuffer(
      fp, stack_top, out_trace, kMaxDepth, 0, /*enable_scanning=*/true);
  for (size_t i = 0; i < depth; ++i) {
    uintptr_t pc = reinterpret_cast<uintptr_t>(out_trace[i]);
    stack->push_back(base::Frame(pc, module_cache()->GetModuleForAddress(pc)));
  }
  return base::UnwindResult::COMPLETED;
}

}  // namespace tracing
