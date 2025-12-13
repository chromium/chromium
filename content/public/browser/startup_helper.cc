// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/startup_helper.h"

#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/browser/startup_helper.h"

namespace content {

void CreateBrowserTaskExecutor() {
  BrowserTaskExecutor::Create();
}

void InstallPartitionAllocSchedulerLoopQuarantineTaskObserver() {
  BrowserTaskExecutor::
      InstallPartitionAllocSchedulerLoopQuarantineTaskObserver();
}

void StartThreadPool() {
  StartBrowserThreadPool();
}

}  // namespace content
