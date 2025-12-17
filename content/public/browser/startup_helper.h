// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STARTUP_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_STARTUP_HELPER_H_

#include "content/common/content_export.h"

// This file is intended to expose some internal //content functions.
// This is done so that we don't expose the more specific headers more broadly
// than needed.

namespace content {

// Creates and registers a BrowserTaskExecutor on the current thread which owns
// a BrowserUIThreadScheduler. This facilitates posting tasks to a BrowserThread
// via //base/task/post_task.h.
CONTENT_EXPORT void CreateBrowserTaskExecutor();

// Installs the partition alloc scheduler loop quarantine task observer.
CONTENT_EXPORT void InstallPartitionAllocSchedulerLoopQuarantineTaskObserver();

// Starts the thread pool based on the field trial param settings.
CONTENT_EXPORT void StartThreadPool();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STARTUP_HELPER_H_
