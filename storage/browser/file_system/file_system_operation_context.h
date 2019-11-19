// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_checker.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/browser/quota/quota_limit_type.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {

class FileSystemContext;

// A context class which is carried around by FileSystemOperation and
// its delegated tasks. It is valid to reuse one context instance across
// multiple operations as far as those operations are supposed to share
// the same context (e.g. use the same task runner, share the quota etc).
// Note that the remaining quota bytes (allowed_bytes_growth) may be
// updated during the execution of write operations.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemOperationContext
    : public base::SupportsUserData {
 public:
  explicit FileSystemOperationContext(FileSystemContext* context);

  // Specifies |task_runner| which the operation is performed on.
  // The backend of |task_runner| must outlive the IO thread.
  FileSystemOperationContext(FileSystemContext* context,
                             base::SequencedTaskRunner* task_runner);

  ~FileSystemOperationContext() override;

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  // Updates the current remaining quota.
  // This can be called to update the remaining quota during the operation.
  void set_allowed_bytes_growth(const int64_t& allowed_bytes_growth) {
    allowed_bytes_growth_ = allowed_bytes_growth;
  }

  // Returns the current remaining quota.
  int64_t allowed_bytes_growth() const { return allowed_bytes_growth_; }
  storage::QuotaLimitType quota_limit_type() const { return quota_limit_type_; }
  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }

  ChangeObserverList* change_observers() { return &change_observers_; }
  UpdateObserverList* update_observers() { return &update_observers_; }

  // Following setters should be called only on the same thread as the
  // FileSystemOperationContext is created (i.e. are not supposed be updated
  // after the context's passed onto other task runners).
  void set_change_observers(const ChangeObserverList& list) {
    DCHECK_CALLED_ON_VALID_THREAD(setter_thread_checker_);
    change_observers_ = list;
  }
  void set_update_observers(const UpdateObserverList& list) {
    DCHECK_CALLED_ON_VALID_THREAD(setter_thread_checker_);
    update_observers_ = list;
  }
  void set_quota_limit_type(storage::QuotaLimitType limit_type) {
    DCHECK_CALLED_ON_VALID_THREAD(setter_thread_checker_);
    quota_limit_type_ = limit_type;
  }

 private:
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The current remaining quota, used by ObfuscatedFileUtil.
  int64_t allowed_bytes_growth_;

  // The current quota limit type, used by ObfuscatedFileUtil.
  storage::QuotaLimitType quota_limit_type_;

  // Observers attached to this context.
  ChangeObserverList change_observers_;
  UpdateObserverList update_observers_;

  // Used to check its setters are not called on arbitrary thread.
  THREAD_CHECKER(setter_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationContext);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_OPERATION_CONTEXT_H_
