// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/native_file_system_underlying_sink.h"

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_write_params.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_writable_file_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"

namespace blink {

NativeFileSystemUnderlyingSink::NativeFileSystemUnderlyingSink(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter> writer_remote)
    : writer_remote_(context) {
  writer_remote_.Bind(std::move(writer_remote),
                      context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(writer_remote_.is_bound());
}

ScriptPromise NativeFileSystemUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise NativeFileSystemUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  v8::Local<v8::Value> value = chunk.V8Value();

  ArrayBufferOrArrayBufferViewOrBlobOrUSVStringOrWriteParams input;
  V8ArrayBufferOrArrayBufferViewOrBlobOrUSVStringOrWriteParams::ToImpl(
      script_state->GetIsolate(), value, input,
      UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (input.IsNull()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot provide null object");
    return ScriptPromise();
  }

  if (input.IsWriteParams()) {
    return HandleParams(script_state, std::move(*input.GetAsWriteParams()),
                        exception_state);
  }

  ArrayBufferOrArrayBufferViewOrBlobOrUSVString write_data;
  V8ArrayBufferOrArrayBufferViewOrBlobOrUSVString::ToImpl(
      script_state->GetIsolate(), value, write_data,
      UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return WriteData(script_state, offset_, std::move(write_data),
                   exception_state);
}

ScriptPromise NativeFileSystemUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Object reached an invalid state");
    return ScriptPromise();
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_remote_->Close(WTF::Bind(
      &NativeFileSystemUnderlyingSink::CloseComplete, WrapPersistent(this)));

  return result;
}

ScriptPromise NativeFileSystemUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been aborted. Terminating the remote connection
  // will ensure that the writes are not closed successfully.
  if (writer_remote_.is_bound())
    writer_remote_.reset();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise NativeFileSystemUnderlyingSink::HandleParams(
    ScriptState* script_state,
    const WriteParams& params,
    ExceptionState& exception_state) {
  if (params.type() == "truncate") {
    if (!params.hasSizeNonNull()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "Invalid params passed. truncate requires a size argument");
      return ScriptPromise();
    }
    return Truncate(script_state, params.sizeNonNull(), exception_state);
  }

  if (params.type() == "seek") {
    if (!params.hasPositionNonNull()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "Invalid params passed. seek requires a position argument");
      return ScriptPromise();
    }
    return Seek(script_state, params.positionNonNull(), exception_state);
  }

  if (params.type() == "write") {
    uint64_t position =
        params.hasPositionNonNull() ? params.positionNonNull() : offset_;
    if (!params.hasData()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "Invalid params passed. write requires a data argument");
      return ScriptPromise();
    }
    return WriteData(script_state, position, params.data(), exception_state);
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Object reached an invalid state");
  return ScriptPromise();
}

ScriptPromise NativeFileSystemUnderlyingSink::WriteData(
    ScriptState* script_state,
    uint64_t position,
    const ArrayBufferOrArrayBufferViewOrBlobOrUSVString& data,
    ExceptionState& exception_state) {
  DCHECK(!data.IsNull());

  auto blob_data = std::make_unique<BlobData>();
  Blob* blob = nullptr;
  if (data.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = data.GetAsArrayBuffer();
    blob_data->AppendBytes(array_buffer->Data(), array_buffer->ByteLength());
  } else if (data.IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view = data.GetAsArrayBufferView().View();
    blob_data->AppendBytes(array_buffer_view->BaseAddress(),
                           array_buffer_view->byteLength());
  } else if (data.IsBlob()) {
    blob = data.GetAsBlob();
  } else if (data.IsUSVString()) {
    // Let the developer be explicit about line endings.
    blob_data->AppendText(data.GetAsUSVString(),
                          /*normalize_line_endings_to_native=*/false);
  }

  if (!blob) {
    uint64_t size = blob_data->length();
    blob = MakeGarbageCollected<Blob>(
        BlobDataHandle::Create(std::move(blob_data), size));
  }

  return WriteBlob(script_state, position, blob, exception_state);
}

ScriptPromise NativeFileSystemUnderlyingSink::WriteBlob(
    ScriptState* script_state,
    uint64_t position,
    Blob* blob,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Object reached an invalid state");
    return ScriptPromise();
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_remote_->Write(
      position, blob->AsMojoBlob(),
      WTF::Bind(&NativeFileSystemUnderlyingSink::WriteComplete,
                WrapPersistent(this)));
  return result;
}

ScriptPromise NativeFileSystemUnderlyingSink::Truncate(
    ScriptState* script_state,
    uint64_t size,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Object reached an invalid state");
    return ScriptPromise();
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_remote_->Truncate(
      size, WTF::Bind(&NativeFileSystemUnderlyingSink::TruncateComplete,
                      WrapPersistent(this), size));
  return result;
}

ScriptPromise NativeFileSystemUnderlyingSink::Seek(
    ScriptState* script_state,
    uint64_t offset,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Object reached an invalid state");
    return ScriptPromise();
  }
  offset_ = offset;
  return ScriptPromise::CastUndefined(script_state);
}

void NativeFileSystemUnderlyingSink::WriteComplete(
    mojom::blink::NativeFileSystemErrorPtr result,
    uint64_t bytes_written) {
  DCHECK(pending_operation_);
  native_file_system_error::ResolveOrReject(pending_operation_, *result);
  pending_operation_ = nullptr;

  if (result->status == mojom::blink::NativeFileSystemStatus::kOk) {
    // Advance offset.
    offset_ += bytes_written;
  }
}

void NativeFileSystemUnderlyingSink::TruncateComplete(
    uint64_t to_size,
    mojom::blink::NativeFileSystemErrorPtr result) {
  DCHECK(pending_operation_);
  native_file_system_error::ResolveOrReject(pending_operation_, *result);
  pending_operation_ = nullptr;

  if (result->status == mojom::blink::NativeFileSystemStatus::kOk) {
    // Set offset to smallest last set size so that a subsequent write is not
    // out of bounds.
    offset_ = to_size < offset_ ? to_size : offset_;
  }
}

void NativeFileSystemUnderlyingSink::CloseComplete(
    mojom::blink::NativeFileSystemErrorPtr result) {
  DCHECK(pending_operation_);
  native_file_system_error::ResolveOrReject(pending_operation_, *result);
  pending_operation_ = nullptr;
  // We close the mojo pipe because we intend this writable file stream to be
  // discarded after close. Subsequent operations will fail.
  writer_remote_.reset();
}

void NativeFileSystemUnderlyingSink::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  UnderlyingSinkBase::Trace(visitor);
  visitor->Trace(writer_remote_);
  visitor->Trace(pending_operation_);
}

}  // namespace blink
