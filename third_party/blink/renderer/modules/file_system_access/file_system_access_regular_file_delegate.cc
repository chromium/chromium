// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_regular_file_delegate.h"

#include "base/files/file_error_or.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#endif

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
    :
#if defined(OS_MAC)
      context_(context),
      file_utilities_host_(context),
#endif  // defined(OS_MAC)
      backing_file_(std::move(backing_file)),
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {
}

base::FileErrorOr<int> FileSystemAccessRegularFileDelegate::Read(
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

base::FileErrorOr<int> FileSystemAccessRegularFileDelegate::Write(
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
    base::OnceCallback<void(base::FileErrorOr<int64_t>)> callback) {
  auto wrapped_callback =
      CrossThreadOnceFunction<void(base::FileErrorOr<int64_t>)>(
          std::move(callback));

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
    WTF::CrossThreadOnceFunction<void(base::FileErrorOr<int64_t>)>
        wrapped_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  int64_t length = delegate->backing_file_.GetLength();

  // If the length is negative, the file operation failed. Get the last error
  // now before another file operation might run.
  base::FileErrorOr<int64_t> result =
      length >= 0 ? length : base::File::GetLastFileError();

  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(std::move(wrapped_callback), std::move(result)));
}

void FileSystemAccessRegularFileDelegate::SetLength(
    int64_t length,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (length < 0) {
    // This method is expected to finish asynchronously, so post a task to the
    // current sequence to return the error.
    task_runner_->PostTask(
        FROM_HERE, WTF::Bind(std::move(callback),
                             base::File::Error::FILE_ERROR_INVALID_OPERATION));
    return;
  }

#if defined(OS_MAC)
  // On macOS < 10.15, a sandboxing limitation causes failures in ftruncate()
  // syscalls issued from renderers. For this reason, base::File::SetLength()
  // fails in the renderer. We work around this problem by calling ftruncate()
  // in the browser process. See https://crbug.com/1084565.
  if (!base::mac::IsAtLeastOS10_15()) {
    if (!file_utilities_host_.is_bound()) {
      context_->GetBrowserInterfaceBroker().GetInterface(
          file_utilities_host_.BindNewPipeAndPassReceiver(task_runner_));
    }
    file_utilities_host_->SetLength(
        std::move(backing_file_), length,
        WTF::Bind(&FileSystemAccessRegularFileDelegate::DidSetLengthMac,
                  WrapPersistent(this), std::move(callback)));
    return;
  }
#endif  // defined(OS_MAC)

  auto wrapped_callback =
      CrossThreadOnceFunction<void(bool)>(std::move(callback));

  // Truncate file on a worker thread and reply back to this sequence.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&FileSystemAccessRegularFileDelegate::DoSetLength,
                          WrapCrossThreadPersistent(this),
                          std::move(wrapped_callback), task_runner_, length));
}

// static
void FileSystemAccessRegularFileDelegate::DoSetLength(
    CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
    CrossThreadOnceFunction<void(bool)> wrapped_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int64_t length) {
  bool result = delegate->backing_file_.SetLength(length);
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(std::move(wrapped_callback), std::move(result)));
}

#if defined(OS_MAC)
void FileSystemAccessRegularFileDelegate::DidSetLengthMac(
    base::OnceCallback<void(bool)> callback,
    base::File file,
    bool result) {
  backing_file_ = std::move(file);
  std::move(callback).Run(result);
}

#endif  // defined(OS_MAC)
void FileSystemAccessRegularFileDelegate::Flush(
    base::OnceCallback<void(bool)> callback) {
  auto wrapped_callback =
      CrossThreadOnceFunction<void(bool)>(std::move(callback));

  // Flush file on a worker thread and reply back to this sequence.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&FileSystemAccessRegularFileDelegate::DoFlush,
                          WrapCrossThreadPersistent(this),
                          std::move(wrapped_callback), task_runner_));
}

// static
void FileSystemAccessRegularFileDelegate::DoFlush(
    CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
    CrossThreadOnceFunction<void(bool)> wrapped_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  bool result = delegate->backing_file_.Flush();
  PostCrossThreadTask(*task_runner, FROM_HERE,
                      CrossThreadBindOnce(std::move(wrapped_callback), result));
}

void FileSystemAccessRegularFileDelegate::Close(base::OnceClosure callback) {
  auto wrapped_callback = CrossThreadOnceClosure(std::move(callback));

  // Close file on a worker thread and reply back to this sequence.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&FileSystemAccessRegularFileDelegate::DoClose,
                          WrapCrossThreadPersistent(this),
                          std::move(wrapped_callback), task_runner_));
}

// static
void FileSystemAccessRegularFileDelegate::DoClose(
    CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
    CrossThreadOnceClosure wrapped_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  delegate->backing_file_.Close();
  PostCrossThreadTask(*task_runner, FROM_HERE, std::move(wrapped_callback));
}

}  // namespace blink
