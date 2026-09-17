// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/window_idle_tasks.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/scheduler/window_idle_tasks_manager.h"

namespace blink {

uint32_t WindowIdleTasks::requestIdleCallback(
    LocalDOMWindow& window,
    V8IdleRequestCallback* callback,
    const IdleRequestOptions* options) {
  return WindowIdleTasksManager::From(window).RequestIdleCallback(
      callback, options, base::PassKey<WindowIdleTasks>{});
}

void WindowIdleTasks::cancelIdleCallback(LocalDOMWindow& window, uint32_t id) {
  WindowIdleTasksManager::From(window).CancelIdleCallback(
      id, base::PassKey<WindowIdleTasks>{});
}

}  // namespace blink
