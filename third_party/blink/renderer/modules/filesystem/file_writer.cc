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
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

static const int kMaxRecursionDepth = 3;
static const double kProgressNotificationIntervalMS = 50;

FileWriter* FileWriter::Create(ExecutionContext* context) {
  return new FileWriter(context);
}

FileWriter::FileWriter(ExecutionContext* context)
    : ContextLifecycleObserver(context),
      ready_state_(kInit),
      operation_in_progress_(kOperationNone),
      queued_operation_(kOperationNone),
      bytes_written_(0),
      bytes_to_write_(0),
      truncate_length_(-1),
      num_aborts_(0),
      recursion_depth_(0),
      last_progress_notification_time_ms_(0),
      request_id_(0) {}

FileWriter::~FileWriter() {
  DCHECK(!recursion_depth_);
}

const AtomicString& FileWriter::InterfaceName() const {
  return EventTargetNames::FileWriter;
}

void FileWriter::ContextDestroyed(ExecutionContext*) {
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
  DCHECK_EQ(truncate_length_, -1);
  if (ready_state_ == kWriting) {
    SetError(FileError::kInvalidStateErr, exception_state);
    return;
  }
  if (recursion_depth_ > kMaxRecursionDepth) {
    SetError(FileError::kSecurityErr, exception_state);
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

  FireEvent(EventTypeNames::writestart);
}

void FileWriter::seek(long long position, ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;
  if (ready_state_ == kWriting) {
    SetError(FileError::kInvalidStateErr, exception_state);
    return;
  }

  DCHECK_EQ(truncate_length_, -1);
  DCHECK_EQ(queued_operation_, kOperationNone);
  SeekInternal(position);
}

void FileWriter::truncate(long long position, ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;
  DCHECK_EQ(truncate_length_, -1);
  if (ready_state_ == kWriting || position < 0) {
    SetError(FileError::kInvalidStateErr, exception_state);
    return;
  }
  if (recursion_depth_ > kMaxRecursionDepth) {
    SetError(FileError::kSecurityErr, exception_state);
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
  FireEvent(EventTypeNames::writestart);
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
  DCHECK_EQ(-1, truncate_length_);
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

  long long num_aborts = num_aborts_;
  // We could get an abort in the handler for this event. If we do, it's
  // already handled the cleanup and signalCompletion call.
  double now = CurrentTimeMS();
  if (complete || !last_progress_notification_time_ms_ ||
      (now - last_progress_notification_time_ms_ >
       kProgressNotificationIntervalMS)) {
    last_progress_notification_time_ms_ = now;
    FireEvent(EventTypeNames::progress);
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
  DCHECK_GE(truncate_length_, 0);
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
      .Truncate(path, offset, &request_id_,
                WTF::Bind(&FileWriter::DidFinish, WrapWeakPersistent(this)));
}

void FileWriter::DoWrite(const KURL& path,
                         const String& blob_id,
                         int64_t offset) {
  FileSystemDispatcher::From(GetExecutionContext())
      .Write(
          path, blob_id, offset, &request_id_,
          WTF::BindRepeating(&FileWriter::DidWrite, WrapWeakPersistent(this)),
          WTF::Bind(&FileWriter::DidFinish, WrapWeakPersistent(this)));
}

void FileWriter::DoCancel() {
  FileSystemDispatcher::From(GetExecutionContext())
      .Cancel(request_id_,
              WTF::Bind(&FileWriter::DidFinish, WrapWeakPersistent(this)));
}

void FileWriter::CompleteAbort() {
  DCHECK_EQ(operation_in_progress_, kOperationAbort);
  operation_in_progress_ = kOperationNone;
  Operation operation = queued_operation_;
  queued_operation_ = kOperationNone;
  DoOperation(operation);
}

void FileWriter::DoOperation(Operation operation) {
  probe::AsyncTaskScheduled(GetExecutionContext(), "FileWriter", this);
  switch (operation) {
    case kOperationWrite:
      DCHECK_EQ(kOperationNone, operation_in_progress_);
      DCHECK_EQ(-1, truncate_length_);
      DCHECK(blob_being_written_.Get());
      DCHECK_EQ(kWriting, ready_state_);
      Write(position(), blob_being_written_->Uuid());
      break;
    case kOperationTruncate:
      DCHECK_EQ(kOperationNone, operation_in_progress_);
      DCHECK_GE(truncate_length_, 0);
      DCHECK_EQ(kWriting, ready_state_);
      Truncate(truncate_length_);
      break;
    case kOperationNone:
      DCHECK_EQ(kOperationNone, operation_in_progress_);
      DCHECK_EQ(-1, truncate_length_);
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
      truncate_length_ = -1;
      break;
  }
  DCHECK_EQ(queued_operation_, kOperationNone);
  operation_in_progress_ = operation;
}

void FileWriter::SignalCompletion(base::File::Error error) {
  ready_state_ = kDone;
  truncate_length_ = -1;
  if (error != base::File::FILE_OK) {
    error_ = FileError::CreateDOMException(error);
    if (base::File::FILE_ERROR_ABORT == error)
      FireEvent(EventTypeNames::abort);
    else
      FireEvent(EventTypeNames::error);
  } else
    FireEvent(EventTypeNames::write);
  FireEvent(EventTypeNames::writeend);

  probe::AsyncTaskCanceled(GetExecutionContext(), this);
}

void FileWriter::FireEvent(const AtomicString& type) {
  probe::AsyncTask async_task(GetExecutionContext(), this);
  ++recursion_depth_;
  DispatchEvent(
      *ProgressEvent::Create(type, true, bytes_written_, bytes_to_write_));
  --recursion_depth_;
  DCHECK_GE(recursion_depth_, 0);
}

void FileWriter::SetError(FileError::ErrorCode error_code,
                          ExceptionState& exception_state) {
  DCHECK(error_code);
  FileError::ThrowDOMException(exception_state, error_code);
  error_ = FileError::CreateDOMException(error_code);
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

void FileWriter::Trace(blink::Visitor* visitor) {
  visitor->Trace(error_);
  visitor->Trace(blob_being_written_);
  EventTargetWithInlineData::Trace(visitor);
  FileWriterBase::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
