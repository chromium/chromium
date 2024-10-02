// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_MAC)
#include "net/base/apple/guarded_fd.h"
#endif  // BUILDFLAG(IS_MAC)

namespace net {

namespace {

void CallInt64ToInt(CompletionOnceCallback callback, int64_t result) {
  std::move(callback).Run(static_cast<int>(result));
}

}  // namespace

FileStream::Context::IOResult::IOResult()
    : result(OK),
      os_error(0) {
}

FileStream::Context::IOResult::IOResult(int64_t result,
                                        logging::SystemErrorCode os_error)
    : result(result), os_error(os_error) {
}

// static
FileStream::Context::IOResult FileStream::Context::IOResult::FromOSError(
    logging::SystemErrorCode os_error) {
  return IOResult(MapSystemError(os_error), os_error);
}

// ---------------------------------------------------------------------

FileStream::Context::OpenResult::OpenResult() = default;

FileStream::Context::OpenResult::OpenResult(base::File file,
                                            IOResult error_code)
    : file(std::move(file)), error_code(error_code) {}

FileStream::Context::OpenResult::OpenResult(OpenResult&& other)
    : file(std::move(other.file)), error_code(other.error_code) {}

FileStream::Context::OpenResult& FileStream::Context::OpenResult::operator=(
    OpenResult&& other) {
  file = std::move(other.file);
  error_code = other.error_code;
  return *this;
}

// ---------------------------------------------------------------------

void FileStream::Context::Orphan() {
  DCHECK(!orphaned_);

  orphaned_ = true;

  if (!async_in_progress_) {
    CloseAndDelete();
  } else if (file_.IsValid()) {
#if BUILDFLAG(IS_WIN)
    CancelIo(file_.GetPlatformFile());
#endif
  }
}

void FileStream::Context::Open(const base::FilePath& path,
                               int open_flags,
                               CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::OpenFileImpl, base::Unretained(this), path,
                     open_flags),
      base::BindOnce(&Context::OnOpenCompleted, base::Unretained(this),
                     std::move(callback)));
  DCHECK(posted);

  async_in_progress_ = true;
}

void FileStream::Context::Close(CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::CloseFileImpl, base::Unretained(this)),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));
  DCHECK(posted);

  async_in_progress_ = true;
}

void FileStream::Context::Seek(int64_t offset,
                               Int64CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  if (offset < 0) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
    return;
  }

  bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::SeekFileImpl, base::Unretained(this), offset),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     std::move(callback)));
  DCHECK(posted);

  async_in_progress_ = true;
}

void FileStream::Context::GetFileInfo(base::File::Info* file_info,
                                      CompletionOnceCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::GetFileInfoImpl, base::Unretained(this),
                     base::Unretained(file_info)),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));

  async_in_progress_ = true;
}

void FileStream::Context::Flush(CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Context::FlushFileImpl, base::Unretained(this)),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));
  DCHECK(posted);

  async_in_progress_ = true;
}

bool FileStream::Context::IsOpen() const {
  return file_.IsValid();
}

FileStream::Context::OpenResult FileStream::Context::OpenFileImpl(
    const base::FilePath& path, int open_flags) {
#if BUILDFLAG(IS_POSIX)
  // Always use blocking IO.
  open_flags &= ~base::File::FLAG_ASYNC;
#endif
  // FileStream::Context actually closes the file asynchronously,
  // independently from FileStream's destructor. It can cause problems for
  // users wanting to delete the file right after FileStream deletion. Thus
  // we are always adding SHARE_DELETE flag to accommodate such use case.
  // TODO(rvargas): This sounds like a bug, as deleting the file would
  // presumably happen on the wrong thread. There should be an async delete.
#if BUILDFLAG(IS_WIN)
  open_flags |= base::File::FLAG_WIN_SHARE_DELETE;
#endif
  base::File file(path, open_flags);
  if (!file.IsValid()) {
    return OpenResult(base::File(),
                      IOResult::FromOSError(logging::GetLastSystemErrorCode()));
  }

  return OpenResult(std::move(file), IOResult(OK, 0));
}

FileStream::Context::IOResult FileStream::Context::GetFileInfoImpl(
    base::File::Info* file_info) {
  bool result = file_.GetInfo(file_info);
  if (!result)
    return IOResult::FromOSError(logging::GetLastSystemErrorCode());
  return IOResult(OK, 0);
}

FileStream::Context::IOResult FileStream::Context::CloseFileImpl() {
#if BUILDFLAG(IS_MAC)
  // https://crbug.com/330771755: Guard against a file descriptor being closed
  // out from underneath the file.
  if (file_.IsValid()) {
    guardid_t guardid = reinterpret_cast<guardid_t>(this);
    PCHECK(change_fdguard_np(file_.GetPlatformFile(), &guardid,
                             GUARD_CLOSE | GUARD_DUP,
                             /*nguard=*/nullptr, /*nguardflags=*/0,
                             /*fdflagsp=*/nullptr) == 0);
  }
#endif
  file_.Close();
  return IOResult(OK, 0);
}

FileStream::Context::IOResult FileStream::Context::FlushFileImpl() {
  if (file_.Flush())
    return IOResult(OK, 0);

  return IOResult::FromOSError(logging::GetLastSystemErrorCode());
}

void FileStream::Context::OnOpenCompleted(CompletionOnceCallback callback,
                                          OpenResult open_result) {
  file_ = std::move(open_result.file);
  if (file_.IsValid() && !orphaned_)
    OnFileOpened();

#if BUILDFLAG(IS_MAC)
  // https://crbug.com/330771755: Guard against a file descriptor being closed
  // out from underneath the file.
  if (file_.IsValid()) {
    guardid_t guardid = reinterpret_cast<guardid_t>(this);
    PCHECK(change_fdguard_np(file_.GetPlatformFile(), /*guard=*/nullptr,
                             /*guardflags=*/0, &guardid,
                             GUARD_CLOSE | GUARD_DUP,
                             /*fdflagsp=*/nullptr) == 0);
  }
#endif

  OnAsyncCompleted(IntToInt64(std::move(callback)), open_result.error_code);
}

void FileStream::Context::CloseAndDelete() {
  DCHECK(!async_in_progress_);

  if (file_.IsValid()) {
    bool posted = task_runner_.get()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&Context::CloseFileImpl),
                                  base::Owned(this)));
    DCHECK(posted);
  } else {
    delete this;
  }
}

Int64CompletionOnceCallback FileStream::Context::IntToInt64(
    CompletionOnceCallback callback) {
  return base::BindOnce(&CallInt64ToInt, std::move(callback));
}

void FileStream::Context::OnAsyncCompleted(Int64CompletionOnceCallback callback,
                                           const IOResult& result) {
  // Reset this before Run() as Run() may issue a new async operation. Also it
  // should be reset before Close() because it shouldn't run if any async
  // operation is in progress.
  async_in_progress_ = false;
  if (orphaned_) {
    CloseAndDelete();
  } else {
    std::move(callback).Run(result.result);
  }
}

}  // namespace net
