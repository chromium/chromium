// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/process_state.h"

#include "base/lazy_instance.h"

namespace blink {
namespace scheduler {
namespace internal {

namespace {

base::LazyInstance<ProcessState>::Leaky g_process_state;

}  // namespace

// static
ProcessState* ProcessState::Get() {
  return g_process_state.Pointer();
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
