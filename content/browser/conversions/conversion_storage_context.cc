// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_context.h"

#include "base/task/lazy_thread_pool_task_runner.h"
#include "content/browser/conversions/conversion_storage_sql.h"

namespace content {

namespace {

// The shared-task runner for all conversion storage operations. Note that
// different ConversionStorageContext perform operations on the same task
// runner. This prevents any potential races when a given context is destroyed
// and recreated for the same backing storage. This uses
// BLOCK_SHUTDOWN as some data deletion operations may be running when the
// browser is closed, and we want to ensure all data is deleted correctly.
base::LazyThreadPoolSequencedTaskRunner g_storage_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                         base::MayBlock(),
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

}  // namespace

ConversionStorageContext::ConversionStorageContext(
    const base::FilePath& user_data_directory,
    std::unique_ptr<ConversionStorageDelegateImpl> delegate,
    const base::Clock* clock)
    : storage_(
          base::SequenceBound<ConversionStorageSql>(g_storage_task_runner.Get(),
                                                    user_data_directory,
                                                    std::move(delegate),
                                                    clock)) {}

ConversionStorageContext::~ConversionStorageContext() = default;

}  // namespace content
