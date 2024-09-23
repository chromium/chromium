/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/file_writer_sync.h"

#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void FileWriterSync::write(Blob* data, ExceptionState& exception_state) {
  DCHECK(data);
  DCHECK(complete_);

  PrepareForWrite();
  Write(position(), *data);
  DCHECK(complete_);
  if (error_) {
    file_error::ThrowDOMException(exception_state, error_);
    return;
  }
  SetPosition(position() + data->size());
  if (position() > length())
    SetLength(position());
}

void FileWriterSync::seek(int64_t position, ExceptionState& exception_state) {
  DCHECK(complete_);
  SeekInternal(position);
}

void FileWriterSync::truncate(int64_t offset, ExceptionState& exception_state) {
  DCHECK(complete_);
  if (offset < 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      file_error::kInvalidStateErrorMessage);
    return;
  }
  PrepareForWrite();
  Truncate(offset);
  DCHECK(complete_);
  if (error_) {
    file_error::ThrowDOMException(exception_state, error_);
    return;
  }
  if (offset < position())
    SetPosition(offset);
  SetLength(offset);
}

void FileWriterSync::DidWriteImpl(int64_t bytes, bool complete) {
  DCHECK_EQ(base::File::FILE_OK, error_);
  DCHECK(!complete_);
  complete_ = complete;
}

void FileWriterSync::DidTruncateImpl() {
  DCHECK_EQ(base::File::FILE_OK, error_);
  DCHECK(!complete_);
  complete_ = true;
}

void FileWriterSync::DidFailImpl(base::File::Error error) {
  DCHECK_EQ(base::File::FILE_OK, error_);
  error_ = error;
  DCHECK(!complete_);
  complete_ = true;
}

void FileWriterSync::DoTruncate(const KURL& path, int64_t offset) {
  if (!GetExecutionContext())
    return;
  FileSystemDispatcher::From(GetExecutionContext())
      .TruncateSync(
          path, offset,
          WTF::BindOnce(&FileWriterSync::DidFinish, WrapWeakPersistent(this)));
}

void FileWriterSync::DoWrite(const KURL& path,
                             const Blob& blob,
                             int64_t offset) {
  if (!GetExecutionContext())
    return;
  FileSystemDispatcher::From(GetExecutionContext())
      .WriteSync(
          path, blob, offset,
          WTF::BindRepeating(&FileWriterSync::DidWrite,
                             WrapWeakPersistent(this)),
          WTF::BindOnce(&FileWriterSync::DidFinish, WrapWeakPersistent(this)));
}

void FileWriterSync::DoCancel() {
  NOTREACHED_IN_MIGRATION();
}

FileWriterSync::FileWriterSync(ExecutionContext* context)
    : ExecutionContextClient(context),
      error_(base::File::FILE_OK),
      complete_(true) {}

void FileWriterSync::PrepareForWrite() {
  DCHECK(complete_);
  error_ = base::File::FILE_OK;
  complete_ = false;
}

FileWriterSync::~FileWriterSync() = default;

void FileWriterSync::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  FileWriterBase::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
