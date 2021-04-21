// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_STORAGE_PARTITION_TEST_HELPERS_H_
#define CONTENT_TEST_STORAGE_PARTITION_TEST_HELPERS_H_

#include "base/callback.h"

namespace content {
class StoragePartition;

// Replaces the SharedWorkerService implementation with a test-specific one that
// tracks running shared workers.
void InjectTestSharedWorkerService(StoragePartition* storage_partition);

// Terminates all workers and notifies when complete. This is used for
// testing when it is important to make sure that all shared worker activity
// has stopped. Can only be used if InjectTestSharedWorkerService() was called.
void TerminateAllSharedWorkers(StoragePartition* storage_partition,
                               base::OnceClosure callback);

}  // namespace content

#endif  // CONTENT_TEST_STORAGE_PARTITION_TEST_HELPERS_H_
