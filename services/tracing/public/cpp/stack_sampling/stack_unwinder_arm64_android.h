// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_UNWINDER_ARM64_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_UNWINDER_ARM64_ANDROID_H_

#include "base/component_export.h"
#include "base/profiler/unwinder.h"

namespace tracing {

// Unwinder implementation for arm64 builds with frame pointer enabled.
class COMPONENT_EXPORT(TRACING_CPP) UnwinderArm64 : public base::Unwinder {
 public:
  bool CanUnwindFrom(const base::Frame& current_frame) const override;

  base::UnwindResult TryUnwind(base::RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<base::Frame>* stack) const override;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_UNWINDER_ARM64_ANDROID_H_
