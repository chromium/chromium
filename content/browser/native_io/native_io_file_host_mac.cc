// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker.h"
#include "content/browser/native_io/native_io_file_host.h"

#include <utility>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/browser/native_io/native_io_manager.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

namespace content {

namespace {

using blink::mojom::NativeIOError;
using blink::mojom::NativeIOErrorType;

struct SetLengthResult {
  base::File file;
  int64_t actual_length;
  base::File::Error error;
};

// Performs the file I/O work in SetLength().
SetLengthResult DoSetLength(const int64_t length, base::File file) {
  DCHECK_GE(length, 0) << "The file length should not be negative";
  SetLengthResult result;
  bool success = file.SetLength(length);
  result.file = std::move(file);
  result.error = success ? base::File::FILE_OK : file.GetLastFileError();
  result.actual_length = success ? length : file.GetLength();

  return result;
}

void DidSetLength(blink::mojom::NativeIOFileHost::SetLengthCallback callback,
                  SetLengthResult result) {
  blink::mojom::NativeIOErrorPtr error =
      NativeIOManager::FileErrorToNativeIOError(result.error);
  std::move(callback).Run(std::move(result.file), result.actual_length,
                          std::move(error));
}

}  // namespace

void NativeIOFileHost::SetLength(const int64_t length,
                                 base::File file,
                                 SetLengthCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!allow_set_length_ipc_) {
    mojo::ReportBadMessage("SetLength() disabled on this OS.");
    // No error message is specified as the ReportBadMessage() call should close
    // the pipe and kill the renderer.
    std::move(callback).Run(
        std::move(file), /*actual_file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kUnknown, ""));
    return;
  }

  if (length < 0) {
    mojo::ReportBadMessage("The file length cannot be negative.");
    // No error message is specified as the ReportBadMessage() call should close
    // the pipe and kill the renderer.
    std::move(callback).Run(
        std::move(file), /*actual_file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kUnknown, ""));
    return;
  }

  // file.IsValid() does not interact with the file system, so we may call it on
  // this thread.
  if (!file.IsValid()) {
    mojo::ReportBadMessage("The file is invalid.");
    // No error message is specified as the ReportBadMessage() call should close
    // the pipe and kill the renderer.
    std::move(callback).Run(
        std::move(file), /*actual_file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kUnknown, ""));
    return;
  }

  // The NativeIO specification says that calling I/O methods on a file
  // concurrently should result in errors. This check is done in the
  // renderer. We don't need to replicate the check in the browser because
  // performing operations concurrently is not a security issue. A misbehaving
  // renderer would merely be exposed to OS-specific behavior.
  //
  // TaskTraits mirror those of the TaskRunner in NativeIOHost, i.e.,
  // base::MayBlock() for file I/O, base::TaskPriority::USER_VISIBLE as some
  // database operations might be blocking, and
  // base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, as blocking shutdown is not
  // appropriate, yet CONTINUE_ON_SHUTDOWN might require more careful analysis.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DoSetLength, length, std::move(file)),
      base::BindOnce(&DidSetLength, std::move(callback)));
}

}  // namespace content
