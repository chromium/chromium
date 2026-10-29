// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_data_source_bridge.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "media/base/data_source.h"

namespace media {

// State for the single in-flight read, shared via `base::RefCountedData`
// between `Read()` on the blocking sequence and the completion callback passed
// to `DataSource::Read()`. It is created once and reused for every read. If
// `Shutdown()` unblocks `Read()` while `DataSource` still holds the callback,
// the callback's reference keeps `buffer` valid until `DataSource` completes or
// drops it.
struct SymphoniaDataSourceBridge::ReadOperation {
  std::vector<uint8_t> buffer;
  base::WaitableEvent done{base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED};
  int bytes_read = 0;
};

void SymphoniaDataSourceBridge::Deleter::operator()(
    SymphoniaDataSourceBridge* bridge) const {
  if (!bridge) {
    return;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      bridge->blocking_task_runner_;
  bridge->Shutdown();
  if (task_runner->RunsTasksInCurrentSequence()) {
    delete bridge;
    return;
  }
  // Same as `DeleteSoon()`, which can't be used here because the destructor is
  // private.
  task_runner->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce([](SymphoniaDataSourceBridge* doomed) { delete doomed; },
                     bridge));
}

// static
SymphoniaDataSourceBridge::UniquePtr SymphoniaDataSourceBridge::Create(
    DataSource* data_source,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    base::RepeatingClosure error_cb) {
  CHECK(data_source);
  CHECK(blocking_task_runner);
  CHECK(error_cb);
  return UniquePtr(new SymphoniaDataSourceBridge(
      data_source, std::move(blocking_task_runner),
      base::BindPostTaskToCurrentDefault(std::move(error_cb))));
}

SymphoniaDataSourceBridge::SymphoniaDataSourceBridge(
    DataSource* data_source,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    base::RepeatingClosure error_cb)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      error_cb_(std::move(error_cb)),
      is_streaming_(data_source->IsStreaming()),
      data_source_(data_source),
      aborted_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED),
      read_op_(base::MakeRefCounted<base::RefCountedData<ReadOperation>>()) {
  // Created on the owning sequence and used on `blocking_task_runner_`.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SymphoniaDataSourceBridge::~SymphoniaDataSourceBridge() {
  // Shutting down must stop `data_source_` completely first and schedule
  // destruction on `blocking_task_runner_` so no `Read()` frame is still on the
  // stack when the bridge is destroyed. `Deleter` enforces both invariants.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  CHECK(aborted_.IsSignaled());
}

void SymphoniaDataSourceBridge::Shutdown() {
  DataSource* data_source = nullptr;
  {
    base::AutoLock lock(lock_);
    data_source = data_source_;
    data_source_ = nullptr;
  }
  if (data_source) {
    data_source->Stop();
  }
  aborted_.Signal();
}

int64_t SymphoniaDataSourceBridge::Read(rust::Slice<uint8_t> buf) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  if (buf.empty()) {
    return 0;
  }
  // `aborted_` is never reset, so once it is signaled `read_op_` is never
  // touched again. This keeps a read that `Shutdown()` interrupted from having
  // its buffer resized while `DataSource` may still write into it.
  if (aborted_.IsSignaled()) {
    return -1;
  }

  ReadOperation& op = read_op_->data;
  if (op.buffer.size() < buf.size()) {
    op.buffer.resize(buf.size());
  }
  const base::span<uint8_t> read_span = base::span(op.buffer).first(buf.size());
  op.done.Reset();

  {
    base::AutoLock lock(lock_);
    if (!data_source_) {
      return -1;
    }

    int64_t file_size = 0;
    if (data_source_->GetSize(&file_size) && read_position_ >= file_size) {
      return 0;
    }

    data_source_->Read(
        read_position_, read_span,
        base::BindOnce(
            [](scoped_refptr<base::RefCountedData<ReadOperation>> op,
               int bytes_read) {
              op->data.bytes_read = bytes_read;
              op->data.done.Signal();
            },
            read_op_));
  }

  std::array events = {&aborted_, &op.done};
  const size_t index = base::WaitableEvent::WaitMany(events);
  if (events[index] == &aborted_) {
    // The pending `DataSource` callback holds its own reference to `read_op_`,
    // so `op.buffer` stays valid until `DataSource` completes or drops it.
    return -1;
  }

  const int bytes_read = op.bytes_read;
  if (bytes_read == DataSource::kReadError) {
    aborted_.Signal();
    bool should_report_error = false;
    {
      base::AutoLock lock(lock_);
      should_report_error = data_source_ != nullptr;
    }
    if (should_report_error) {
      error_cb_.Run();
    }
    return -1;
  }

  if (bytes_read < 0) {
    // `DataSource::kAborted` cancels only the current read (for example during
    // `CancelPendingSeek()`), leaving the bridge usable for subsequent seeks
    // and reads.
    return -1;
  }

  const size_t count = std::min(buf.size(), static_cast<size_t>(bytes_read));
  if (count > 0) {
    base::span(buf).first(count).copy_from(read_span.first(count));
    read_position_ =
        base::CheckAdd(read_position_, count).ValueOrDie<int64_t>();
  }
  return static_cast<int64_t>(count);
}

bool SymphoniaDataSourceBridge::Seek(uint64_t pos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  if (!base::IsValueInRangeForNumericType<int64_t>(pos) ||
      aborted_.IsSignaled()) {
    return false;
  }

  const int64_t signed_pos = static_cast<int64_t>(pos);
  {
    base::AutoLock lock(lock_);
    int64_t file_size = 0;
    if (!data_source_ ||
        (data_source_->GetSize(&file_size) && signed_pos > file_size)) {
      return false;
    }
  }

  read_position_ = signed_pos;
  return true;
}

uint64_t SymphoniaDataSourceBridge::GetPosition() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  return static_cast<uint64_t>(read_position_);
}

int64_t SymphoniaDataSourceBridge::GetLength() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock lock(lock_);
  int64_t size = 0;
  if (!data_source_ || !data_source_->GetSize(&size)) {
    return -1;
  }
  return size;
}

bool SymphoniaDataSourceBridge::IsSeekable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  return !is_streaming_;
}

}  // namespace media
