// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/cppgc.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "gin/gin_features.h"
#include "gin/public/v8_platform.h"
#include "v8/include/cppgc/platform.h"

namespace gin {

namespace {

int g_init_count = 0;

}  // namespace

void InitializeCppgcFromV8Platform() {
  static constexpr size_t kRegularCageSize =
      static_cast<size_t>(4) * 1024 * 1024 * 1024;
  static constexpr size_t kLargerCageSize =
      static_cast<size_t>(16) * 1024 * 1024 * 1024;

  DCHECK_GE(g_init_count, 0);
  if (g_init_count++ > 0) {
    return;
  }

  size_t desired_cage_size = kRegularCageSize;
  auto overridden_state = base::FeatureList::GetStateIfOverridden(
      features::kV8CppGCEnableLargerCage);
  if (overridden_state.has_value()) {
    if (overridden_state.value()) {
      desired_cage_size = kLargerCageSize;
    } else {
    }
  }

  cppgc::InitializeProcess(gin::V8Platform::Get()->GetPageAllocator(),
                           desired_cage_size);
}

void MaybeShutdownCppgc() {
  DCHECK_GT(g_init_count, 0);
  if (--g_init_count > 0)
    return;

  cppgc::ShutdownProcess();
}

}  // namespace gin
