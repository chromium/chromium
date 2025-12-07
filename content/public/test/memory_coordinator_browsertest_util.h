// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_

namespace content::test {

// Note: For both of these functions, all MemoryConsumers, including those
// registered in child processes, will be invoked. The call will be synchronous
// iff the MemoryConsumer is registered on the main thread of the browser
// process. In other cases, the call will be asynchronous.

// Calls `ReleaseMemory()` on all registered MemoryConsumers.
void NotifyReleaseMemory();

// Calls `UpdateMemoryLimit(percentage) on all registered MemoryConsumers.
void NotifyUpdateMemoryLimit(int percentage);

}  // namespace content::test

#endif  // CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_
