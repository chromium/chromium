// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_capacity_tracker.h"

#include "base/bits.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_modification_host.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {
// Minimum size of an allocation requested from the browser.
constexpr int64_t kMinAllocationSize = 1024 * 1024;
// Maximum size until which the allocation strategy doubles the requested
// allocation.
constexpr int64_t kMaxAllocationDoublingSize = 128 * kMinAllocationSize;
}  // namespace

namespace blink {

FileSystemAccessCapacityTracker::FileSystemAccessCapacityTracker(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileModificationHost>
        file_modification_host_remote,
    int64_t file_size,
    base::PassKey<FileSystemAccessRegularFileDelegate>)
    : file_modification_host_(context),
      file_size_(file_size),
      file_capacity_(file_size) {
  file_modification_host_.Bind(std::move(file_modification_host_remote),
                               context->GetTaskRunner(TaskType::kStorage));
  DCHECK(file_modification_host_.is_bound());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FileSystemAccessCapacityTracker::RequestFileCapacityChange(
    int64_t required_capacity,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(file_capacity_, 0);
  DCHECK_GE(required_capacity, 0);

  int64_t requested_capacity = GetNextCapacityRequestSize(required_capacity);
  DCHECK_GE(requested_capacity, required_capacity);

  // This static assertion checks that subtracting a non-negative int64_t value
  // from another one will not overflow.
  static_assert(0 - std::numeric_limits<int64_t>::max() >=
                    std::numeric_limits<int64_t>::min(),
                "The `capacity_delta` computation below may overflow");
  // Since `requested_capacity` and `file_capacity_` are nonnegative, the
  // arithmetic will not overflow.
  int64_t capacity_delta = requested_capacity - file_capacity_;
  if (capacity_delta <= 0) {
    std::move(callback).Run(true);
    return;
  }
  file_modification_host_->RequestCapacityChange(
      capacity_delta,
      WTF::BindOnce(&FileSystemAccessCapacityTracker::DidRequestCapacityChange,
                    WrapPersistent(this), required_capacity,
                    std::move(callback)));
}

bool FileSystemAccessCapacityTracker::RequestFileCapacityChangeSync(
    int64_t required_capacity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(file_capacity_, 0);
  DCHECK_GE(required_capacity, 0);

  int64_t requested_capacity = GetNextCapacityRequestSize(required_capacity);
  DCHECK_GE(requested_capacity, required_capacity);

  // This static assertion checks that subtracting a non-negative int64_t value
  // from another one will not overflow.
  static_assert(0 - std::numeric_limits<int64_t>::max() >=
                    std::numeric_limits<int64_t>::min(),
                "The `capacity_delta` computation below may overflow");
  // Since `requested_capacity` and `file_capacity_` are nonnegative, the
  // arithmetic will not overflow.
  int64_t capacity_delta = requested_capacity - file_capacity_;
  if (capacity_delta <= 0)
    return true;

  int64_t granted_capacity;
  // Request the necessary capacity from the browser process.
  bool call_succeeded = file_modification_host_->RequestCapacityChange(
      capacity_delta, &granted_capacity);
  DCHECK(call_succeeded) << "Mojo call failed";

  bool capacity_change_successful =
      base::CheckAdd(file_capacity_, granted_capacity)
          .AssignIfValid(&file_capacity_);
  DCHECK(capacity_change_successful)
      << "Mojo call returned out-of-bounds capacity";
  return file_capacity_ >= required_capacity;
}

void FileSystemAccessCapacityTracker::OnFileContentsModified(int64_t new_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(file_size_, 0) << "A file's size should never be negative.";
  DCHECK_GE(file_capacity_, file_size_)
      << "A file's capacity should never be smaller than its size.";
  DCHECK_GE(new_size, 0) << "A file's size should never be negative.";

  file_size_ = new_size;

  file_modification_host_->OnContentsModified();
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

// static
int64_t FileSystemAccessCapacityTracker::GetNextCapacityRequestSize(
    int64_t required_capacity) {
  DCHECK_GE(required_capacity, 0);
  if (required_capacity <= kMinAllocationSize)
    return kMinAllocationSize;
  if (required_capacity <= kMaxAllocationDoublingSize) {
    // The assertion makes sure that casting `required_capacity` succeeds.
    static_assert(
        kMaxAllocationDoublingSize <= std::numeric_limits<uint32_t>::max(),
        "The allocation strategy will overflow.");
    // Since the previous statements ensured that `required_capacity` <=
    // `kMaxAllocationDoublingSize`
    // <= std::numeric_limits<uint32_t>::max() , the cast always succeeds.
    // This computes (in LaTeX notation) 2^{\ceil{\log_2(r)}}, where r is
    // `required_capacity`. The bit shift performs the exponentiation.
    return 1 << base::bits::Log2Ceiling(
               static_cast<uint32_t>(required_capacity));
  }
  // The next statements compute (in LaTeX notation) m \cdot \ceil{\frac{r}{m}},
  // where m is `kMaxAllocationDoublingSize` and r is `required_capacity`.
  int64_t numerator_plus_one;
  int64_t multiplier;
  int64_t requested_capacity;
  if (!base::CheckAdd(required_capacity, kMaxAllocationDoublingSize)
           .AssignIfValid(&numerator_plus_one)) {
    return required_capacity;
  }
  if (!base::CheckDiv(numerator_plus_one - 1, kMaxAllocationDoublingSize)
           .AssignIfValid(&multiplier)) {
    return required_capacity;
  }
  if (!base::CheckMul(kMaxAllocationDoublingSize, multiplier)
           .AssignIfValid(&requested_capacity)) {
    return required_capacity;
  }
  return requested_capacity;
}

}  // namespace blink
