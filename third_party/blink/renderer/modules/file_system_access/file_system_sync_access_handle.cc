// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"

#include "base/files/file_error_or.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected_macros.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    ExecutionContext* context,
    FileSystemAccessFileDelegate* file_delegate,
    mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
        access_handle_remote,
    V8FileSystemSyncAccessHandleMode lock_mode)
    : file_delegate_(file_delegate),
      access_handle_remote_(context),
      lock_mode_(std::move(lock_mode)) {
  access_handle_remote_.Bind(std::move(access_handle_remote),
                             context->GetTaskRunner(TaskType::kStorage));
  DCHECK(access_handle_remote_.is_bound());
}

void FileSystemSyncAccessHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(file_delegate_);
  visitor->Trace(access_handle_remote_);
}

void FileSystemSyncAccessHandle::close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closed_ || !access_handle_remote_.is_bound()) {
    // close() is idempotent.
    return;
  }

  DCHECK(file_delegate_->IsValid()) << "file delgate invalidated before close";

  is_closed_ = true;
  file_delegate_->Close();
  access_handle_remote_->Close();
}

void FileSystemSyncAccessHandle::flush(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return;
  }

  if (lock_mode_.AsEnum() ==
      V8FileSystemSyncAccessHandleMode::Enum::kReadOnly) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "Cannot write to access handle in 'read-only' mode");
    return;
  }

  DCHECK(file_delegate_->IsValid()) << "file delgate invalidated before flush";

  bool success = file_delegate()->Flush();
  if (!success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "flush failed");
  }
}

uint64_t FileSystemSyncAccessHandle::getSize(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return 0;
  }

  DCHECK(file_delegate_->IsValid())
      << "file delgate invalidated before getSize";

  ASSIGN_OR_RETURN(
      const int64_t length, file_delegate()->GetLength(), [&](auto) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "getSize failed");
        return 0;
      });
  return base::as_unsigned(length);
}

void FileSystemSyncAccessHandle::truncate(uint64_t size,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return;
  }

  if (lock_mode_.AsEnum() ==
      V8FileSystemSyncAccessHandleMode::Enum::kReadOnly) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "Cannot write to access handle in 'read-only' mode");
    return;
  }

  DCHECK(file_delegate_->IsValid())
      << "file delgate invalidated before truncate";

  if (!base::CheckedNumeric<int64_t>(size).IsValid()) {
    exception_state.ThrowTypeError("Cannot truncate file to given length");
    return;
  }

  RETURN_IF_ERROR(
      file_delegate()->SetLength(size), [&](base::File::Error error) {
        if (error == base::File::FILE_ERROR_NO_SPACE) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kQuotaExceededError,
              "No space available for this operation");
        } else if (error != base::File::FILE_OK) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidStateError, "truncate failed");
        }
      });
  cursor_ = std::min(cursor_, size);
}

uint64_t FileSystemSyncAccessHandle::read(const AllowSharedBufferSource* buffer,
                                          FileSystemReadWriteOptions* options,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file_delegate()->IsValid() || is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  uint64_t file_offset = options->hasAt() ? options->at() : cursor_;
  if (!base::CheckedNumeric<int64_t>(file_offset).IsValid()) {
    exception_state.ThrowTypeError("Cannot read at given offset");
    return 0;
  }

  ASSIGN_OR_RETURN(
      int result, file_delegate()->Read(file_offset, AsByteSpan(*buffer)),
      [&](auto) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "Failed to read the content");
        return 0;
      });
  uint64_t bytes_read = base::as_unsigned(result);
  // This is guaranteed to not overflow since `file_offset` is a positive
  // int64_t and `result` is a positive int, while `cursor_` is a uint64_t.
  bool cursor_position_is_valid =
      base::CheckAdd(file_offset, bytes_read).AssignIfValid(&cursor_);
  DCHECK(cursor_position_is_valid) << "cursor position could not be determined";
  return bytes_read;
}

uint64_t FileSystemSyncAccessHandle::write(
    const AllowSharedBufferSource* buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file_delegate()->IsValid() || is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  if (lock_mode_.AsEnum() ==
      V8FileSystemSyncAccessHandleMode::Enum::kReadOnly) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "Cannot write to access handle in 'read-only' mode");
    return 0;
  }

  uint64_t file_offset = options->hasAt() ? options->at() : cursor_;
  if (!base::CheckedNumeric<int64_t>(file_offset).IsValid()) {
    exception_state.ThrowTypeError("Cannot write at given offset");
    return 0;
  }

  auto buffer_span = AsByteSpan(*buffer);
  size_t write_size = buffer_span.size();
  if (!base::CheckedNumeric<int>(write_size).IsValid()) {
    exception_state.ThrowTypeError("Cannot write more than 2GB");
  }

  int64_t write_end_offset;
  if (!base::CheckAdd(file_offset, write_size)
           .AssignIfValid(&write_end_offset)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kQuotaExceededError,
        "No capacity available for this operation");
    return 0;
  }
  DCHECK_GE(write_end_offset, 0);

  ASSIGN_OR_RETURN(
      int result, file_delegate()->Write(file_offset, buffer_span),
      [&](base::File::Error error) {
        DCHECK_NE(error, base::File::FILE_OK);
        if (error == base::File::FILE_ERROR_NO_SPACE) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kQuotaExceededError,
              "No space available for this operation");
        } else {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidStateError,
              "Failed to write to the access handle");
        }
        return 0;
      });

  uint64_t bytes_written = base::as_unsigned(result);
  // This is guaranteed to not overflow since `file_offset` is a positive
  // int64_t and `result` is a positive int, while `cursor_` is a uint64_t.
  bool cursor_position_is_valid =
      base::CheckAdd(file_offset, bytes_written).AssignIfValid(&cursor_);
  DCHECK(cursor_position_is_valid) << "cursor position could not be determined";
  return bytes_written;
}

String FileSystemSyncAccessHandle::mode() {
  return lock_mode_.AsString();
}

}  // namespace blink
