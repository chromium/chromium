// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_capacity_tracker.h"

#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_capacity_allocation_host.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemAccessCapacityTracker::FileSystemAccessCapacityTracker(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessCapacityAllocationHost>
        capacity_allocation_host_remote,
    int64_t file_size,
    base::PassKey<FileSystemAccessRegularFileDelegate>)
    : capacity_allocation_host_(context),
      file_size_(file_size),
      file_capacity_(file_size),
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {
  capacity_allocation_host_.Bind(std::move(capacity_allocation_host_remote),
                                 task_runner_);
  DCHECK(capacity_allocation_host_.is_bound());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FileSystemAccessCapacityTracker::RequestFileCapacityChange(
    int64_t required_capacity,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(file_capacity_, 0);
  DCHECK_GE(required_capacity, 0);

  // This static assertion checks that subtracting a non-negative int64_t value
  // from another one will not overflow.
  static_assert(0 - std::numeric_limits<int64_t>::max() >=
                    std::numeric_limits<int64_t>::min(),
                "The `capacity_delta` computation below may overflow");
  // Since `required_capacity` and `file_capacity_` are nonnegative, the
  // arithmetic will not overflow.
  int64_t capacity_delta = required_capacity - file_capacity_;
  if (capacity_delta <= 0) {
    std::move(callback).Run(true);
    return;
  }
  // TODO(https://crbug.com/1240056): Implement a more sophisticated strategy
  // for determining allocation size.
  capacity_allocation_host_->RequestCapacityChange(
      capacity_delta,
      WTF::Bind(&FileSystemAccessCapacityTracker::DidRequestCapacityChange,
                WrapPersistent(this), required_capacity, std::move(callback)));
}

bool FileSystemAccessCapacityTracker::RequestFileCapacityChangeSync(
    int64_t required_capacity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(file_capacity_, 0);
  DCHECK_GE(required_capacity, 0);

  // This static assertion checks that subtracting a non-negative int64_t value
  // from another one will not overflow.
  static_assert(0 - std::numeric_limits<int64_t>::max() >=
                    std::numeric_limits<int64_t>::min(),
                "The `needed_capacity` computation below may overflow");
  // Since `required_capacity` and `file_capacity_` are nonnegative, the
  // arithmetic will not overflow.
  int64_t capacity_delta = required_capacity - file_capacity_;
  if (capacity_delta <= 0)
    return true;

  // TODO(https://crbug.com/1240056): Implement a more sophisticated strategy
  // for determining allocation size.
  int64_t granted_capacity;
  // Request the necessary capacity from the browser process.
  bool call_succeeded = capacity_allocation_host_->RequestCapacityChange(
      capacity_delta, &granted_capacity);
  DCHECK(call_succeeded) << "Mojo call failed";

  bool capacity_change_successful =
      base::CheckAdd(file_capacity_, granted_capacity)
          .AssignIfValid(&file_capacity_);
  DCHECK(capacity_change_successful)
      << "Mojo call returned out-of-bounds capacity";
  return file_capacity_ >= required_capacity;
}

void FileSystemAccessCapacityTracker::CommitFileSizeChange(int64_t new_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(file_size_, 0) << "A file's size should never be negative.";
  DCHECK_GE(file_capacity_, file_size_)
      << "A file's capacity should never be smaller than its size.";
  DCHECK_GE(new_size, 0) << "A file's size should never be negative.";

  file_size_ = new_size;
}

void FileSystemAccessCapacityTracker::DidRequestCapacityChange(
    int64_t required_capacity,
    base::OnceCallback<void(bool)> callback,
    int64_t granted_capacity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool capacity_change_successful =
      base::CheckAdd(file_capacity_, granted_capacity)
          .AssignIfValid(&file_capacity_);
  DCHECK(capacity_change_successful)
      << "Mojo call returned out-of-bounds capacity";
  bool sufficient_capacity_granted = required_capacity <= file_capacity_;
  std::move(callback).Run(sufficient_capacity_granted);
}

}  // namespace blink
