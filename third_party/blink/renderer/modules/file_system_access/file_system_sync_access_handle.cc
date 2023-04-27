// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"

#include "base/files/file_error_or.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    ExecutionContext* context,
    FileSystemAccessFileDelegate* file_delegate,
    mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
        access_handle_remote)
    : file_delegate_(file_delegate), access_handle_remote_(context) {
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

  base::FileErrorOr<int64_t> error_or_length = file_delegate()->GetLength();
  if (!error_or_length.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "getSize failed");
    return 0;
  }
  return base::as_unsigned(error_or_length.value());
}

void FileSystemSyncAccessHandle::truncate(uint64_t size,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return;
  }

  DCHECK(file_delegate_->IsValid())
      << "file delgate invalidated before truncate";

  if (!base::CheckedNumeric<int64_t>(size).IsValid()) {
    exception_state.ThrowTypeError("Cannot truncate file to given length");
    return;
  }

  base::FileErrorOr<bool> result = file_delegate()->SetLength(size);
  if (!result.has_value()) {
    if (result.error() == base::File::FILE_ERROR_NO_SPACE) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kQuotaExceededError,
          "No space available for this operation");
    } else if (result.error() != base::File::FILE_OK) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "truncate failed");
    }
    return;
  }
  cursor_ = std::min(cursor_, size);
}

uint64_t FileSystemSyncAccessHandle::read(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file_delegate()->IsValid() || is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  size_t read_size = buffer->byteLength();
  uint8_t* read_data = static_cast<uint8_t*>(buffer->BaseAddressMaybeShared());
  uint64_t file_offset = options->hasAt() ? options->at() : cursor_;
  if (!base::CheckedNumeric<int64_t>(file_offset).IsValid()) {
    exception_state.ThrowTypeError("Cannot read at given offset");
    return 0;
  }

  base::FileErrorOr<int> result =
      file_delegate()->Read(file_offset, {read_data, read_size});

  if (!result.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to read the content");
    return 0;
  }
  uint64_t bytes_read = base::as_unsigned(result.value());
  // This is guaranteed to not overflow since `file_offset` is a positive
  // int64_t and `result` is a positive int, while `cursor_` is a uint64_t.
  bool cursor_position_is_valid =
      base::CheckAdd(file_offset, bytes_read).AssignIfValid(&cursor_);
  DCHECK(cursor_position_is_valid) << "cursor position could not be determined";
  return bytes_read;
}

uint64_t FileSystemSyncAccessHandle::write(
    MaybeShared<DOMArrayBufferView> buffer,
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
    exception_state.ThrowTypeError("Cannot write at given offset");
    return 0;
  }

  size_t write_size = buffer->byteLength();
  if (!base::CheckedNumeric<int>(write_size).IsValid()) {
    exception_state.ThrowTypeError("Cannot write more than 2GB");
  }

  uint8_t* write_data = static_cast<uint8_t*>(buffer->BaseAddressMaybeShared());

  int64_t write_end_offset;
  if (!base::CheckAdd(file_offset, write_size)
           .AssignIfValid(&write_end_offset)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kQuotaExceededError,
        "No capacity available for this operation");
    return 0;
  }
  DCHECK_GE(write_end_offset, 0);

  base::FileErrorOr<int> result =
      file_delegate()->Write(file_offset, {write_data, write_size});
  if (!result.has_value()) {
    base::File::Error file_error = result.error();
    DCHECK_NE(file_error, base::File::FILE_OK);
    if (file_error == base::File::FILE_ERROR_NO_SPACE) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kQuotaExceededError,
          "No space available for this operation");
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Failed to write to the access handle");
    }
    return 0;
  }

  uint64_t bytes_written = base::as_unsigned(result.value());
  // This is guaranteed to not overflow since `file_offset` is a positive
  // int64_t and `result` is a positive int, while `cursor_` is a uint64_t.
  bool cursor_position_is_valid =
      base::CheckAdd(file_offset, bytes_written).AssignIfValid(&cursor_);
  DCHECK(cursor_position_is_valid) << "cursor position could not be determined";
  return bytes_written;
}

}  // namespace blink
