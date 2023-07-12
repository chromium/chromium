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

#include "third_party/blink/renderer/modules/filesystem/file_writer.h"

#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

static const int kMaxRecursionDepth = 3;
static const base::TimeDelta kProgressNotificationInterval =
    base::Milliseconds(50);
static constexpr uint64_t kMaxTruncateLength =
    std::numeric_limits<uint64_t>::max();

FileWriter::FileWriter(ExecutionContext* context)
    : ActiveScriptWrappable<FileWriter>({}),
      ExecutionContextLifecycleObserver(context),
      ready_state_(kInit),
      operation_in_progress_(kOperationNone),
      queued_operation_(kOperationNone),
      bytes_written_(0),
      bytes_to_write_(0),
      truncate_length_(kMaxTruncateLength),
      num_aborts_(0),
      recursion_depth_(0),
      request_id_(0) {}

FileWriter::~FileWriter() {
  DCHECK(!recursion_depth_);
}

const AtomicString& FileWriter::InterfaceName() const {
  return event_target_names::kFileWriter;
}

void FileWriter::ContextDestroyed() {
  Dispose();
}

bool FileWriter::HasPendingActivity() const {
  return operation_in_progress_ != kOperationNone ||
         queued_operation_ != kOperationNone || ready_state_ == kWriting;
}

void FileWriter::write(Blob* data, ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;
  DCHECK(data);
  DCHECK_EQ(truncate_length_, kMaxTruncateLength);
  if (ready_state_ == kWriting) {
    SetError(FileErrorCode::kInvalidStateErr, exception_state);
    return;
  }
  if (recursion_depth_ > kMaxRecursionDepth) {
    SetError(FileErrorCode::kSecurityErr, exception_state);
    return;
  }

  blob_being_written_ = data;
  ready_state_ = kWriting;
  bytes_written_ = 0;
  bytes_to_write_ = data->size();
  DCHECK_EQ(queued_operation_, kOperationNone);
  if (operation_in_progress_ != kOperationNone) {
    // We must be waiting for an abort to complete, since m_readyState wasn't
    // kWriting.
    DCHECK_EQ(operation_in_progress_, kOperationAbort);
    queued_operation_ = kOperationWrite;
  } else
    DoOperation(kOperationWrite);

  FireEvent(event_type_names::kWritestart);
}

void FileWriter::seek(int64_t position, ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;
  if (ready_state_ == kWriting) {
    SetError(FileErrorCode::kInvalidStateErr, exception_state);
    return;
  }

  DCHECK_EQ(truncate_length_, kMaxTruncateLength);
  DCHECK_EQ(queued_operation_, kOperationNone);
  SeekInternal(position);
}

void FileWriter::truncate(int64_t position, ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;
  DCHECK_EQ(truncate_length_, kMaxTruncateLength);
  if (ready_state_ == kWriting || position < 0) {
    SetError(FileErrorCode::kInvalidStateErr, exception_state);
    return;
  }
  if (recursion_depth_ > kMaxRecursionDepth) {
    SetError(FileErrorCode::kSecurityErr, exception_state);
    return;
  }

  ready_state_ = kWriting;
  bytes_written_ = 0;
  bytes_to_write_ = 0;
  truncate_length_ = position;
  DCHECK_EQ(queued_operation_, kOperationNone);
  if (operation_in_progress_ != kOperationNone) {
    // We must be waiting for an abort to complete, since m_readyState wasn't
    // kWriting.
    DCHECK_EQ(operation_in_progress_, kOperationAbort);
    queued_operation_ = kOperationTruncate;
  } else
    DoOperation(kOperationTruncate);
  FireEvent(event_type_names::kWritestart);
}

void FileWriter::abort(ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;
  if (ready_state_ != kWriting)
    return;
  ++num_aborts_;

  DoOperation(kOperationAbort);
  SignalCompletion(base::File::FILE_ERROR_ABORT);
}

void FileWriter::DidWriteImpl(int64_t bytes, bool complete) {
  if (operation_in_progress_ == kOperationAbort) {
    CompleteAbort();
    return;
  }
  DCHECK_EQ(kWriting, ready_state_);
  DCHECK_EQ(kMaxTruncateLength, truncate_length_);
  DCHECK_EQ(kOperationWrite, operation_in_progress_);
  DCHECK(!bytes_to_write_ || bytes + bytes_written_ > 0);
  DCHECK(bytes + bytes_written_ <= bytes_to_write_);
  bytes_written_ += bytes;
  DCHECK((bytes_written_ == bytes_to_write_) || !complete);
  SetPosition(position() + bytes);
  if (position() > length())
    SetLength(position());
  if (complete) {
    blob_being_written_.Clear();
    operation_in_progress_ = kOperationNone;
  }

  uint64_t num_aborts = num_aborts_;
  // We could get an abort in the handler for this event. If we do, it's
  // already handled the cleanup and signalCompletion call.
  base::TimeTicks now = base::TimeTicks::Now();
  if (complete || !last_progress_notification_time_.is_null() ||
      (now - last_progress_notification_time_ >
       kProgressNotificationInterval)) {
    last_progress_notification_time_ = now;
    FireEvent(event_type_names::kProgress);
  }

  if (complete) {
    if (num_aborts == num_aborts_)
      SignalCompletion(base::File::FILE_OK);
  }
}

void FileWriter::DidTruncateImpl() {
  if (operation_in_progress_ == kOperationAbort) {
    CompleteAbort();
    return;
  }
  DCHECK_EQ(operation_in_progress_, kOperationTruncate);
  DCHECK_NE(truncate_length_, kMaxTruncateLength);
  SetLength(truncate_length_);
  if (position() > length())
    SetPosition(length());
  operation_in_progress_ = kOperationNone;
  SignalCompletion(base::File::FILE_OK);
}

void FileWriter::DidFailImpl(base::File::Error error) {
  DCHECK_NE(kOperationNone, operation_in_progress_);
  DCHECK_NE(base::File::FILE_OK, error);
  if (operation_in_progress_ == kOperationAbort) {
    CompleteAbort();
    return;
  }
  DCHECK_EQ(kOperationNone, queued_operation_);
  DCHECK_EQ(kWriting, ready_state_);
  blob_being_written_.Clear();
  operation_in_progress_ = kOperationNone;
  SignalCompletion(error);
}

void FileWriter::DoTruncate(const KURL& path, int64_t offset) {
  FileSystemDispatcher::From(GetExecutionContext())
      .Truncate(
          path, offset, &request_id_,
          WTF::BindOnce(&FileWriter::DidFinish, WrapWeakPersistent(this)));
}

void FileWriter::DoWrite(const KURL& path, const Blob& blob, int64_t offset) {
  FileSystemDispatcher::From(GetExecutionContext())
      .Write(
          path, blob, offset, &request_id_,
          WTF::BindRepeating(&FileWriter::DidWrite, WrapWeakPersistent(this)),
          WTF::BindOnce(&FileWriter::DidFinish, WrapWeakPersistent(this)));
}

void FileWriter::DoCancel() {
  FileSystemDispatcher::From(GetExecutionContext())
      .Cancel(request_id_,
              WTF::BindOnce(&FileWriter::DidFinish, WrapWeakPersistent(this)));
}

void FileWriter::CompleteAbort() {
  DCHECK_EQ(operation_in_progress_, kOperationAbort);
  operation_in_progress_ = kOperationNone;
  Operation operation = queued_operation_;
  queued_operation_ = kOperationNone;
  DoOperation(operation);
}

void FileWriter::DoOperation(Operation operation) {
  async_task_context_.Schedule(GetExecutionContext(), "FileWriter");
  switch (operation) {
    case kOperationWrite:
      DCHECK_EQ(kOperationNone, operation_in_progress_);
      DCHECK_EQ(kMaxTruncateLength, truncate_length_);
      DCHECK(blob_being_written_.Get());
      DCHECK_EQ(kWriting, ready_state_);
      Write(position(), *blob_being_written_);
      break;
    case kOperationTruncate:
      DCHECK_EQ(kOperationNone, operation_in_progress_);
      DCHECK_NE(truncate_length_, kMaxTruncateLength);
      DCHECK_EQ(kWriting, ready_state_);
      Truncate(truncate_length_);
      break;
    case kOperationNone:
      DCHECK_EQ(kOperationNone, operation_in_progress_);
      DCHECK_EQ(kMaxTruncateLength, truncate_length_);
      DCHECK(!blob_being_written_.Get());
      DCHECK_EQ(kDone, ready_state_);
      break;
    case kOperationAbort:
      if (operation_in_progress_ == kOperationWrite ||
          operation_in_progress_ == kOperationTruncate)
        Cancel();
      else if (operation_in_progress_ != kOperationAbort)
        operation = kOperationNone;
      queued_operation_ = kOperationNone;
      blob_being_written_.Clear();
      truncate_length_ = kMaxTruncateLength;
      break;
  }
  DCHECK_EQ(queued_operation_, kOperationNone);
  operation_in_progress_ = operation;
}

void FileWriter::SignalCompletion(base::File::Error error) {
  ready_state_ = kDone;
  truncate_length_ = kMaxTruncateLength;
  if (error != base::File::FILE_OK) {
    error_ = file_error::CreateDOMException(error);
    if (base::File::FILE_ERROR_ABORT == error)
      FireEvent(event_type_names::kAbort);
    else
      FireEvent(event_type_names::kError);
  } else {
    FireEvent(event_type_names::kWrite);
  }
  FireEvent(event_type_names::kWriteend);

  async_task_context_.Cancel();
}

void FileWriter::FireEvent(const AtomicString& type) {
  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_);
  ++recursion_depth_;
  DispatchEvent(
      *ProgressEvent::Create(type, true, bytes_written_, bytes_to_write_));
  DCHECK_GT(recursion_depth_, 0);
  --recursion_depth_;
}

void FileWriter::SetError(FileErrorCode error_code,
                          ExceptionState& exception_state) {
  DCHECK_NE(error_code, FileErrorCode::kOK);
  file_error::ThrowDOMException(exception_state, error_code);
  error_ = file_error::CreateDOMException(error_code);
}

void FileWriter::Dispose() {
  // Make sure we've actually got something to stop, and haven't already called
  // abort().
  if (ready_state_ == kWriting) {
    DoOperation(kOperationAbort);
    ready_state_ = kDone;
  }
  // Prevents any queued operations from running after abort completes.
  queued_operation_ = kOperationNone;
}

void FileWriter::Trace(Visitor* visitor) const {
  visitor->Trace(error_);
  visitor->Trace(blob_being_written_);
  EventTarget::Trace(visitor);
  FileWriterBase::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
