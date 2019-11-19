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

#include "third_party/blink/renderer/modules/filesystem/file_writer_base.h"

#include <memory>
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"

namespace blink {

FileWriterBase::~FileWriterBase() = default;

void FileWriterBase::Initialize(const KURL& path, int64_t length) {
  DCHECK_GE(length, 0);
  length_ = length;
  path_ = path;
}

FileWriterBase::FileWriterBase()
    : position_(0),
      operation_(kOperationNone),
      cancel_state_(kCancelNotInProgress) {}

void FileWriterBase::SeekInternal(int64_t position) {
  if (position > length_)
    position = length_;
  else if (position < 0)
    position = length_ + position;
  if (position < 0)
    position = 0;
  position_ = position;
}

void FileWriterBase::Truncate(int64_t length) {
  DCHECK_EQ(kOperationNone, operation_);
  DCHECK_EQ(kCancelNotInProgress, cancel_state_);
  operation_ = kOperationTruncate;
  DoTruncate(path_, length);
}

void FileWriterBase::Write(int64_t position, const String& id) {
  DCHECK_EQ(kOperationNone, operation_);
  DCHECK_EQ(kCancelNotInProgress, cancel_state_);
  operation_ = kOperationWrite;
  DoWrite(path_, id, position);
}

// When we cancel a write/truncate, we always get back the result of the write
// before the result of the cancel, no matter what happens.
// So we'll get back either
//   success [of the write/truncate, in a DidWrite(XXX, true)/DidSucceed() call]
//     followed by failure [of the cancel]; or
//   failure [of the write, either from cancel or other reasons] followed by
//     the result of the cancel.
// In the write case, there could also be queued up non-terminal DidWrite calls
// before any of that comes back, but there will always be a terminal write
// response [success or failure] after them, followed by the cancel result, so
// we can ignore non-terminal write responses, take the terminal write success
// or the first failure as the last write response, then know that the next
// thing to come back is the cancel response.  We only notify the
// AsyncFileWriterClient when it's all over.
void FileWriterBase::Cancel() {
  // Check for the cancel passing the previous operation's return in-flight.
  if (operation_ != kOperationWrite && operation_ != kOperationTruncate)
    return;
  if (cancel_state_ != kCancelNotInProgress)
    return;
  cancel_state_ = kCancelSent;
  DoCancel();
}

void FileWriterBase::DidFinish(base::File::Error error_code) {
  if (error_code == base::File::FILE_OK)
    DidSucceed();
  else
    DidFail(error_code);
}

void FileWriterBase::DidWrite(int64_t bytes, bool complete) {
  DCHECK_EQ(kOperationWrite, operation_);
  switch (cancel_state_) {
    case kCancelNotInProgress:
      if (complete)
        operation_ = kOperationNone;
      DidWriteImpl(bytes, complete);
      break;
    case kCancelSent:
      // This is the success call of the write, which we'll eat, even though
      // it succeeded before the cancel got there.  We accepted the cancel call,
      // so the write will eventually return an error.
      if (complete)
        cancel_state_ = kCancelReceivedWriteResponse;
      break;
    case kCancelReceivedWriteResponse:
    default:
      NOTREACHED();
  }
}

void FileWriterBase::DidSucceed() {
  // Write never gets a DidSucceed call, so this is either a cancel or truncate
  // response.
  switch (cancel_state_) {
    case kCancelNotInProgress:
      // A truncate succeeded, with no complications.
      DCHECK_EQ(kOperationTruncate, operation_);
      operation_ = kOperationNone;
      DidTruncateImpl();
      break;
    case kCancelSent:
      DCHECK_EQ(kOperationTruncate, operation_);
      // This is the success call of the truncate, which we'll eat, even though
      // it succeeded before the cancel got there.  We accepted the cancel call,
      // so the truncate will eventually return an error.
      cancel_state_ = kCancelReceivedWriteResponse;
      break;
    case kCancelReceivedWriteResponse:
      // This is the success of the cancel operation.
      FinishCancel();
      break;
    default:
      NOTREACHED();
  }
}

void FileWriterBase::DidFail(base::File::Error error_code) {
  DCHECK_NE(kOperationNone, operation_);
  switch (cancel_state_) {
    case kCancelNotInProgress:
      // A write or truncate failed.
      operation_ = kOperationNone;
      DidFailImpl(error_code);
      break;
    case kCancelSent:
      // This is the failure of a write or truncate; the next message should be
      // the result of the cancel.  We don't assume that it'll be a success, as
      // the write/truncate could have failed for other reasons.
      cancel_state_ = kCancelReceivedWriteResponse;
      break;
    case kCancelReceivedWriteResponse:
      // The cancel reported failure, meaning that the write or truncate
      // finished before the cancel got there.  But we suppressed the
      // write/truncate's response, and will now report that it was cancelled.
      FinishCancel();
      break;
    default:
      NOTREACHED();
  }
}

void FileWriterBase::FinishCancel() {
  DCHECK_EQ(kCancelReceivedWriteResponse, cancel_state_);
  DCHECK_NE(kOperationNone, operation_);
  cancel_state_ = kCancelNotInProgress;
  operation_ = kOperationNone;
  DidFailImpl(base::File::FILE_ERROR_ABORT);
}

}  // namespace blink
