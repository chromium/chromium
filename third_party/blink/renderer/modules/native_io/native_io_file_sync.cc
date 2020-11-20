// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file_sync.h"

#include <limits>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/native_io/native_io_error.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// Extracts the read/write operation size from the buffer size.
int OperationSize(const DOMArrayBufferView& buffer) {
  // On 32-bit platforms, clamp operation sizes to 2^31-1.
  return base::saturated_cast<int>(buffer.byteLength());
}

NativeIOFileSync::NativeIOFileSync(
    base::File backing_file,
    HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
    ExecutionContext* execution_context)
    : backing_file_(std::move(backing_file)),
      backend_file_(std::move(backend_file)) {
  backend_file_.set_disconnect_handler(WTF::Bind(
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
  // getLength returns an unsigned integer, which is different from e.g.,
  // base::File and POSIX. The uses for negative integers are error handling,
  // which is done through exceptions, and seeking from an offset without type
  // conversions, which is not supported by NativeIO.
  return base::as_unsigned(length);
}

void NativeIOFileSync::setLength(uint64_t length,
                                 ExceptionState& exception_state) {
  if (!base::IsValueInRangeForNumericType<int64_t>(length)) {
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
  mojom::blink::NativeIOErrorPtr set_length_result;

  // Calls to setLength are routed through the browser process, see
  // crbug.com/1084565.
  //
  // We keep a single handle per file, so this handle is passed to the backend
  // and is then given back to the renderer afterwards.
  backend_file_->SetLength(base::as_signed(length), std::move(backing_file_),
                           &backing_file_, &set_length_result);
  DCHECK(backing_file_.IsValid()) << "browser returned closed file";
  if (set_length_result->type != mojom::blink::NativeIOErrorType::kSuccess) {
    ThrowNativeIOWithError(exception_state, std::move(set_length_result));
  }
  return;
}

uint64_t NativeIOFileSync::read(MaybeShared<DOMArrayBufferView> buffer,
                                uint64_t file_offset,
                                ExceptionState& exception_state) {
  int read_size = OperationSize(*buffer.View());
  char* read_data = static_cast<char*>(buffer.View()->BaseAddressMaybeShared());
  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return 0;
  }
  int read_bytes = backing_file_.Read(file_offset, read_data, read_size);
  if (read_bytes < 0) {
    ThrowNativeIOWithError(exception_state, backing_file_.GetLastFileError());
    return 0;
  }
  return base::as_unsigned(read_bytes);
}

uint64_t NativeIOFileSync::write(MaybeShared<DOMArrayBufferView> buffer,
                                 uint64_t file_offset,
                                 ExceptionState& exception_state) {
  int write_size = OperationSize(*buffer.View());
  char* write_data =
      static_cast<char*>(buffer.View()->BaseAddressMaybeShared());
  if (!backing_file_.IsValid()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return 0;
  }
  int written_bytes = backing_file_.Write(file_offset, write_data, write_size);
  if (written_bytes < 0) {
    ThrowNativeIOWithError(exception_state, backing_file_.GetLastFileError());
    return 0;
  }
  return base::as_unsigned(written_bytes);
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
  bool success = backing_file_.Flush();
  if (!success) {
    ThrowNativeIOWithError(exception_state, backing_file_.GetLastFileError());
    return;
  }
  return;
}

void NativeIOFileSync::Trace(Visitor* visitor) const {
  visitor->Trace(backend_file_);
  ScriptWrappable::Trace(visitor);
}

void NativeIOFileSync::OnBackendDisconnect() {
  backend_file_.reset();
  backing_file_.Close();
}

}  // namespace blink
