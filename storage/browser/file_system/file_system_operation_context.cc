// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_operation_context.h"

#include "base/task/sequenced_task_runner.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/quota/quota_limit_type.h"

namespace storage {

FileSystemOperationContext::FileSystemOperationContext(
    FileSystemContext* context)
    : file_system_context_(context),
      task_runner_(file_system_context_->default_file_task_runner()),
      allowed_bytes_growth_(0),
      quota_limit_type_(QuotaLimitType::kUnknown) {}

FileSystemOperationContext::FileSystemOperationContext(
    FileSystemContext* context,
    base::SequencedTaskRunner* task_runner)
    : file_system_context_(context),
      task_runner_(task_runner),
      allowed_bytes_growth_(0),
      quota_limit_type_(QuotaLimitType::kUnknown) {}

FileSystemOperationContext::~FileSystemOperationContext() {
  DETACH_FROM_THREAD(setter_thread_checker_);
}

}  // namespace storage
