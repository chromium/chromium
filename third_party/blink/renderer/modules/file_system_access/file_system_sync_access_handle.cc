// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    ExecutionContext* context,
    FileSystemAccessFileDelegate* file_delegate,
    mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
        access_handle_remote)
    : file_delegate_(file_delegate), access_handle_remote_(context) {
  access_handle_remote_.Bind(
      std::move(access_handle_remote),
      context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(access_handle_remote_.is_bound());
}

ScriptPromise FileSystemSyncAccessHandle::close(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  // TODO(fivedots): Add logic to close file delegate, and deal with
  // closures during IO operations, as done in Storage Foundation API.

  if (!access_handle_remote_.is_bound()) {
    // If the backend went away, no need to tell it that the handle was closed.
    resolver->Resolve();
    return promise;
  }

  access_handle_remote_->Close(
      WTF::Bind([](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
                WrapPersistent(resolver)));
  return promise;
}

void FileSystemSyncAccessHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(file_delegate_);
  visitor->Trace(access_handle_remote_);
}

uint64_t FileSystemSyncAccessHandle::read(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  if (!file_delegate_->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  size_t read_size = buffer->byteLength();
  uint8_t* read_data = static_cast<uint8_t*>(buffer->BaseAddressMaybeShared());
  uint64_t file_offset = options->at();

  FileErrorOr<int> result =
      file_delegate_->Read(file_offset, {read_data, read_size});

  if (result.is_error()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to read the content");
    return 0;
  }
  return base::as_unsigned(result.value());
}

uint64_t FileSystemSyncAccessHandle::write(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  uint64_t file_offset = options->at();
  if (!file_delegate_->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  size_t write_size = buffer->byteLength();
  if (!base::CheckedNumeric<int>(write_size).IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Cannot write more than 2GB");
  }

  uint8_t* write_data = static_cast<uint8_t*>(buffer->BaseAddressMaybeShared());

  int64_t write_end_offset;
  if (!base::CheckAdd(file_offset, write_size)
           .AssignIfValid(&write_end_offset)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "No capacity available for this operation");
    return 0;
  }
  DCHECK_GE(write_end_offset, 0);

  FileErrorOr<int> result =
      file_delegate_->Write(file_offset, {write_data, write_size});
  if (result.is_error()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to write to the access handle");
    return 0;
  }

  return base::as_unsigned(result.value());
}

}  // namespace blink
