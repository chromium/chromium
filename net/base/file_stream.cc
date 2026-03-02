// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include <utility>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "net/base/file_stream_context.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Converts an int result (bytes >= 0 or error < 0) from the internal Context
// to expected<ByteSize, Error>.
base::expected<base::ByteSize, net::Error> IntToReadWriteResult(int result) {
  if (result >= 0) {
    return base::ByteSize(base::checked_cast<uint64_t>(result));
  }
  return base::unexpected(static_cast<net::Error>(result));
}

// Adapts a ReadWriteCallback to a CompletionOnceCallback for internal use.
void RunReadWriteCallback(FileStream::ReadWriteCallback callback, int result) {
  std::move(callback).Run(IntToReadWriteResult(result));
}

// Adapts an ErrorCallback to a CompletionOnceCallback for internal use.
// Casts the int result to net::Error.
void RunErrorCallback(FileStream::ErrorCallback callback, int result) {
  std::move(callback).Run(static_cast<net::Error>(result));
}

}  // namespace

FileStream::FileStream(const scoped_refptr<base::TaskRunner>& task_runner)
    : context_(std::make_unique<Context>(task_runner)) {}

FileStream::FileStream(base::File file,
                       const scoped_refptr<base::TaskRunner>& task_runner)
    : context_(std::make_unique<Context>(std::move(file), task_runner)) {}

FileStream::~FileStream() {
  context_.release()->Orphan();
}

int FileStream::Open(const base::FilePath& path,
                     int open_flags,
                     ErrorCallback callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  DCHECK(open_flags & base::File::FLAG_ASYNC);
  context_->Open(path, open_flags,
                 base::BindOnce(&RunErrorCallback, std::move(callback)));
  return ERR_IO_PENDING;
}

int FileStream::Close(ErrorCallback callback) {
  context_->Close(base::BindOnce(&RunErrorCallback, std::move(callback)));
  return ERR_IO_PENDING;
}

bool FileStream::IsOpen() const {
  return context_->IsOpen();
}

int FileStream::Seek(int64_t offset, SeekCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Seek(offset, std::move(callback));
  return ERR_IO_PENDING;
}

base::expected<base::ByteSize, net::Error>
FileStream::Read(IOBuffer* buf, int buf_len, ReadWriteCallback callback) {
  if (!IsOpen())
    return base::unexpected(ERR_UNEXPECTED);

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  // TODO(hjanuschka): Update FileStream::Context to return
  // base::expected<base::ByteSize, net::Error> directly, eliminating the need
  // for IntToReadWriteResult() and RunReadWriteCallback().
  return IntToReadWriteResult(context_->Read(
      buf, buf_len,
      base::BindOnce(&RunReadWriteCallback, std::move(callback))));
}

base::expected<base::ByteSize, net::Error>
FileStream::Write(IOBuffer* buf, int buf_len, ReadWriteCallback callback) {
  if (!IsOpen())
    return base::unexpected(ERR_UNEXPECTED);

  DCHECK_GE(buf_len, 0);
  // TODO(hjanuschka): Update FileStream::Context to return
  // base::expected<base::ByteSize, net::Error> directly.
  return IntToReadWriteResult(context_->Write(
      buf, buf_len,
      base::BindOnce(&RunReadWriteCallback, std::move(callback))));
}

int FileStream::GetFileInfo(base::File::Info* file_info,
                            ErrorCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->GetFileInfo(file_info,
                        base::BindOnce(&RunErrorCallback, std::move(callback)));
  return ERR_IO_PENDING;
}

int FileStream::Flush(ErrorCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Flush(base::BindOnce(&RunErrorCallback, std::move(callback)));
  return ERR_IO_PENDING;
}

#if BUILDFLAG(IS_WIN)
int FileStream::ConnectNamedPipe(ErrorCallback callback) {
  return IsOpen() ? context_->ConnectNamedPipe(
                        base::BindOnce(&RunErrorCallback, std::move(callback)))
                  : ERR_UNEXPECTED;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace net
