// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file_sync.h"

#include <limits>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "third_party/blink/renderer/modules/native_io/native_io_error.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace blink {

NativeIOFileSync::NativeIOFileSync(
    base::File backing_file,
    int64_t backing_file_length,
    HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
    NativeIOCapacityTracker* capacity_tracker,
    ExecutionContext* execution_context)
    : backing_file_(std::move(backing_file)),
      file_length_(backing_file_length),
      backend_file_(std::move(backend_file)),
      capacity_tracker_(capacity_tracker) {
  DCHECK_GE(backing_file_length, 0);
  DCHECK(capacity_tracker);
  backend_file_.set_disconnect_handler(WTF::BindOnce(
      &NativeIOFileSync::OnBackendDisconnect, WrapWeakPersistent(this)));
}

NativeIOFileSync::~NativeIOFileSync() = default;

void NativeIOFileSync::close() {
  backing_file_.Close();

  if (!backend_file_.is_bound()) {
    // If the backend went away, it already considers the file closed. Nothing
    // to report here.
    return;
  }
  backend_file_->Close();
}

uint64_t NativeIOFileSync::getLength(ExceptionState& exception_state) {
  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return 0;
  }
  int64_t length = backing_file_.GetLength();
  if (length < 0) {
    ThrowNativeIOWithError(exception_state, backing_file_.GetLastFileError());
    return 0;
  }
  DCHECK_EQ(file_length_, length)
      << "The file size should equal the actual length";
  // getLength returns an unsigned integer, which is different from e.g.,
  // base::File and POSIX. The uses for negative integers are error handling,
  // which is done through exceptions, and seeking from an offset without type
  // conversions, which is not supported by NativeIO.
  return base::as_unsigned(length);
}

void NativeIOFileSync::setLength(uint64_t new_length,
                                 ExceptionState& exception_state) {
  if (!base::IsValueInRangeForNumericType<int64_t>(new_length)) {
    // TODO(rstz): Consider throwing QuotaExceededError here.
    exception_state.ThrowTypeError("Length out of bounds");
    return;
  }
  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return;
  }
  int64_t expected_length = base::as_signed(new_length);

  DCHECK_GE(expected_length, 0);
  DCHECK_GE(file_length_, 0);
  static_assert(0 - std::numeric_limits<int32_t>::max() >=
                    std::numeric_limits<int32_t>::min(),
                "The `length_delta` computation below may overflow");
  // Since both values are non-negative, the arithmetic will not overflow.
  int64_t length_delta = expected_length - file_length_;

  if (length_delta > 0) {
    if (!capacity_tracker_->ChangeAvailableCapacity(-length_delta)) {
      ThrowNativeIOWithError(exception_state,
                             mojom::blink::NativeIOError::New(
                                 mojom::blink::NativeIOErrorType::kNoSpace,
                                 "No capacity available for this operation"));
      return;
    }
    file_length_ = expected_length;
  }

#if BUILDFLAG(IS_MAC)
  // On macOS < 10.15, a sandboxing limitation causes failures in ftruncate()
  // syscalls issued from renderers. For this reason, base::File::SetLength()
  // fails in the renderer. We work around this problem by calling ftruncate()
  // in the browser process. See crbug.com/1084565.
  if (!base::mac::IsAtLeastOS10_15()) {
    // Our system has at most one handle to a file, so we can avoid reasoning
    // through the implications of multiple handles pointing to the same file.
    //
    // To preserve this invariant, we pass this file's handle to the browser
    // process during the SetLength() mojo call, and the browser passes it back
    // when the call completes.
    mojom::blink::NativeIOErrorPtr set_length_result;
    int64_t actual_length;
    backend_file_->SetLength(base::as_signed(expected_length),
                             std::move(backing_file_), &backing_file_,
                             &actual_length, &set_length_result);
    DCHECK(backing_file_.IsValid()) << "browser returned closed file";

    if (actual_length >= 0) {
      DCHECK_LE(actual_length, file_length_)
          << "file_length_ should be an upper bound during I/O";
      if (actual_length < file_length_) {
        // For successful length decreases, this logic returns freed up
        // capacity. For unsuccessful length increases, this logic returns
        // unused capacity.
        bool change_success = capacity_tracker_->ChangeAvailableCapacity(
            file_length_ - actual_length);
        DCHECK(change_success) << "Capacity increases should always succeed";
        file_length_ = actual_length;
      }
    } else {
      DCHECK(set_length_result->type !=
             mojom::blink::NativeIOErrorType::kSuccess);
      // base::File::SetLength() failed. Then, attempting to File::GetLength()
      // failed as well. We don't have a reliable measure of the file's length,
      // and the file descriptor is probably unusable. Force-closing the file
      // without reclaiming any capacity minimizes the risk of overusing our
      // allocation.
      backing_file_.Close();
      if (backend_file_.is_bound())
        backend_file_->Close();
    }

    if (set_length_result->type != mojom::blink::NativeIOErrorType::kSuccess) {
      ThrowNativeIOWithError(exception_state, std::move(set_length_result));
      return;
    }
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  base::File::Error set_length_result = base::File::FILE_OK;
  int64_t actual_length = expected_length;

  if (!backing_file_.SetLength(expected_length)) {
    set_length_result = backing_file_.GetLastFileError();
    actual_length = backing_file_.GetLength();
    if (actual_length < 0)
      set_length_result = backing_file_.GetLastFileError();
  }

  if (actual_length >= 0) {
    DCHECK_LE(actual_length, file_length_)
        << "file_length_ should be an upper bound during I/O";
    if (actual_length < file_length_) {
      // For successful length decreases, this logic returns freed up
      // capacity. For unsuccessful length increases, this logic returns
      // unused capacity.
      bool change_success = capacity_tracker_->ChangeAvailableCapacity(
          file_length_ - actual_length);
      DCHECK(change_success) << "Capacity increases should always succeed";
      file_length_ = actual_length;
    }
  } else {
    DCHECK(set_length_result != base::File::FILE_OK);
    // base::File::SetLength() failed. Then, attempting to File::GetLength()
    // failed as well. We don't have a reliable measure of the file's length,
    // and the file descriptor is probably unusable. Force-closing the file
    // without reclaiming any capacity minimizes the risk of overusing our
    // allocation.
    backing_file_.Close();
    if (backend_file_.is_bound())
      backend_file_->Close();
  }
  if (set_length_result != base::File::FILE_OK) {
    ThrowNativeIOWithError(exception_state, set_length_result);
    return;
  }
}

NativeIOReadResult* NativeIOFileSync::read(ScriptState* script_state,
                                           NotShared<DOMArrayBufferView> buffer,
                                           uint64_t file_offset,
                                           ExceptionState& exception_state) {
  int read_size = NativeIOOperationSize(*buffer);

  NativeIOReadResult* read_result = MakeGarbageCollected<NativeIOReadResult>();
  read_result->setReadBytes(0);
  DOMArrayBufferView* result_buffer = TransferToNewArrayBufferView(
      script_state->GetIsolate(), buffer, exception_state);
  if (!result_buffer) {
    return nullptr;
  }
  DCHECK(buffer->IsDetached());

  read_result->setBuffer(NotShared<DOMArrayBufferView>(result_buffer));

  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return read_result;
  }

  char* result_buffer_data = static_cast<char*>(result_buffer->BaseAddress());
  int read_bytes =
      backing_file_.Read(file_offset, result_buffer_data, read_size);
  if (read_bytes < 0) {
    ThrowNativeIOWithError(exception_state, backing_file_.GetLastFileError());
    return read_result;
  }

  read_result->setReadBytes(read_bytes);
  return read_result;
}

NativeIOWriteResult* NativeIOFileSync::write(
    ScriptState* script_state,
    NotShared<DOMArrayBufferView> buffer,
    uint64_t file_offset,
    ExceptionState& exception_state) {
  int write_size = NativeIOOperationSize(*buffer);

  NativeIOWriteResult* write_result =
      MakeGarbageCollected<NativeIOWriteResult>();
  write_result->setWrittenBytes(0);

  DOMArrayBufferView* result_buffer = TransferToNewArrayBufferView(
      script_state->GetIsolate(), buffer, exception_state);
  if (!result_buffer) {
    return nullptr;
  }
  DCHECK(buffer->IsDetached());

  write_result->setBuffer(NotShared<DOMArrayBufferView>(result_buffer));

  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return write_result;
  }

  int64_t write_end_offset;
  if (!base::CheckAdd(file_offset, write_size)
           .AssignIfValid(&write_end_offset)) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kNoSpace,
                               "No capacity available for this operation"));
    return write_result;
  }

  DCHECK_GE(write_end_offset, 0);
  DCHECK_GE(file_length_, 0);
  static_assert(0 - std::numeric_limits<int32_t>::max() >=
                    std::numeric_limits<int32_t>::min(),
                "The `length_delta` computation below may overflow");
  // Since both values are positive, the arithmetic will not overflow.
  int64_t length_delta = write_end_offset - file_length_;
  // The available capacity must be reduced before performing an I/O operation
  // that increases the file length. This prevents double-spending by concurrent
  // I/O operations on different files.
  if (length_delta > 0) {
    if (!capacity_tracker_->ChangeAvailableCapacity(-length_delta)) {
      ThrowNativeIOWithError(exception_state,
                             mojom::blink::NativeIOError::New(
                                 mojom::blink::NativeIOErrorType::kNoSpace,
                                 "No capacity available for this operation"));
      return write_result;
    }
    file_length_ = write_end_offset;
  }

  base::File::Error write_error = base::File::FILE_OK;
  int64_t actual_file_length_on_failure = file_length_;

  char* result_buffer_data = static_cast<char*>(result_buffer->BaseAddress());
  int written_bytes =
      backing_file_.Write(file_offset, result_buffer_data, write_size);
  if (written_bytes < 0) {
    write_error = backing_file_.GetLastFileError();
    actual_file_length_on_failure = backing_file_.GetLength();
  }
  if (write_error != base::File::FILE_OK || written_bytes < write_size) {
    if (actual_file_length_on_failure >= 0) {
      DCHECK_LE(actual_file_length_on_failure, file_length_)
          << "file_length_ should be an upper bound during I/O";
      if (actual_file_length_on_failure < file_length_) {
        bool change_success = capacity_tracker_->ChangeAvailableCapacity(
            file_length_ - actual_file_length_on_failure);
        DCHECK(change_success) << "Capacity increases should always succeed";
        file_length_ = actual_file_length_on_failure;
      }
    } else {
      DCHECK(write_error != base::File::FILE_OK);
      // base::File::Write() failed. Then, attempting to File::GetLength()
      // failed as well. We don't have a reliable measure of the file's length,
      // and the file descriptor is probably unusable. Force-closing the file
      // without reclaiming any capacity minimizes the risk of overusing our
      // allocation.
      backing_file_.Close();
      if (backend_file_.is_bound())
        backend_file_->Close();
    }
  }

  if (write_error != base::File::FILE_OK) {
    ThrowNativeIOWithError(exception_state, write_error);
    return write_result;
  }

  write_result->setWrittenBytes(written_bytes);
  return write_result;
}

void NativeIOFileSync::flush(ExceptionState& exception_state) {
  // This implementation of flush attempts to physically store the data it has
  // written on disk. This behaviour might change in the future.
  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return;
  }
  if (!backing_file_.Flush())
    ThrowNativeIOWithError(exception_state, backing_file_.GetLastFileError());
}

void NativeIOFileSync::Trace(Visitor* visitor) const {
  visitor->Trace(backend_file_);
  visitor->Trace(capacity_tracker_);
  ScriptWrappable::Trace(visitor);
}

void NativeIOFileSync::OnBackendDisconnect() {
  backend_file_.reset();
  backing_file_.Close();
}

}  // namespace blink
