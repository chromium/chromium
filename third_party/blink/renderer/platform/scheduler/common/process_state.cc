// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"

#include "base/lazy_instance.h"

namespace blink {
namespace scheduler {
namespace internal {

namespace {

base::LazyInstance<ProcessState>::Leaky g_process_state =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ProcessState* ProcessState::Get() {
  return g_process_state.Pointer();
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
