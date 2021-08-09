// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_regular_file_delegate.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_error_or.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemAccessFileDelegate* FileSystemAccessFileDelegate::Create(
    ExecutionContext* context,
    base::File backing_file) {
  return MakeGarbageCollected<FileSystemAccessRegularFileDelegate>(
      context, std::move(backing_file),
      base::PassKey<FileSystemAccessFileDelegate>());
}

FileSystemAccessRegularFileDelegate::FileSystemAccessRegularFileDelegate(
    ExecutionContext* context,
    base::File backing_file,
    base::PassKey<FileSystemAccessFileDelegate>)
    : backing_file_(std::move(backing_file)),
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {}

FileErrorOr<int> FileSystemAccessRegularFileDelegate::Read(
    int64_t offset,
    base::span<uint8_t> data) {
  int size = base::checked_cast<int>(data.size());
  int result =
      backing_file_.Read(offset, reinterpret_cast<char*>(data.data()), size);
  if (result >= 0) {
    return result;
  }
  return base::File::GetLastFileError();
}

FileErrorOr<int> FileSystemAccessRegularFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  int size = base::checked_cast<int>(data.size());
  int result =
      backing_file_.Write(offset, reinterpret_cast<char*>(data.data()), size);
  if (size == result) {
    return result;
  }
  return base::File::GetLastFileError();
}

void FileSystemAccessRegularFileDelegate::GetLength(
    base::OnceCallback<void(FileErrorOr<int64_t>)> callback) {
  auto wrapped_callback =
      CrossThreadOnceFunction<void(FileErrorOr<int64_t>)>(std::move(callback));

  // Get file length on a worker thread and reply back to this sequence.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&FileSystemAccessRegularFileDelegate::DoGetLength,
                          WrapCrossThreadPersistent(this),
                          std::move(wrapped_callback), task_runner_));
}

// static
void FileSystemAccessRegularFileDelegate::DoGetLength(
    CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
    WTF::CrossThreadOnceFunction<void(FileErrorOr<int64_t>)> wrapped_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  int64_t length = delegate->backing_file_.GetLength();

  // If the length is negative, the file operation failed. Get the last error
  // now before another file operation might run.
  FileErrorOr<int64_t> result =
      length >= 0 ? length : base::File::GetLastFileError();

  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(std::move(wrapped_callback), std::move(result)));
}

bool FileSystemAccessRegularFileDelegate::SetLength(int64_t length) {
  return backing_file_.SetLength(length);
}

bool FileSystemAccessRegularFileDelegate::Flush() {
  return backing_file_.Flush();
}

void FileSystemAccessRegularFileDelegate::Close() {
  backing_file_.Close();
}

}  // namespace blink
