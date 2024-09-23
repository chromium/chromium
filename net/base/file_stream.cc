// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include <utility>

#include "build/build_config.h"
#include "net/base/file_stream_context.h"
#include "net/base/net_errors.h"

namespace net {

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
                     CompletionOnceCallback callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  DCHECK(open_flags & base::File::FLAG_ASYNC);
  context_->Open(path, open_flags, std::move(callback));
  return ERR_IO_PENDING;
}

int FileStream::Close(CompletionOnceCallback callback) {
  context_->Close(std::move(callback));
  return ERR_IO_PENDING;
}

bool FileStream::IsOpen() const {
  return context_->IsOpen();
}

int FileStream::Seek(int64_t offset, Int64CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Seek(offset, std::move(callback));
  return ERR_IO_PENDING;
}

int FileStream::Read(IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->Read(buf, buf_len, std::move(callback));
}

int FileStream::Write(IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK_GE(buf_len, 0);
  return context_->Write(buf, buf_len, std::move(callback));
}

int FileStream::GetFileInfo(base::File::Info* file_info,
                            CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->GetFileInfo(file_info, std::move(callback));
  return ERR_IO_PENDING;
}

int FileStream::Flush(CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Flush(std::move(callback));
  return ERR_IO_PENDING;
}

#if BUILDFLAG(IS_WIN)
int FileStream::ConnectNamedPipe(CompletionOnceCallback callback) {
  return IsOpen() ? context_->ConnectNamedPipe(std::move(callback))
                  : ERR_UNEXPECTED;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace net
