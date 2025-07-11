// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/memory_coordinator_browsertest_util.h"

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

namespace content::test {

void NotifyReleaseMemory() {
  NotifyReleaseMemoryForTesting();
}

void NotifyUpdateMemoryLimit(int percentage) {
  NotifyUpdateMemoryLimitForTesting(percentage);
}

}  // namespace content::test
