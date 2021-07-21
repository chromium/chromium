// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    ExecutionContext* context,
    FileSystemAccessFileDelegate* file_delegate,
    mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
        access_handle_remote)
    : file_delegate_(file_delegate),
      access_handle_remote_(context),
      resolver_task_runner_(
          context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {
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

ScriptPromise FileSystemSyncAccessHandle::flush(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!EnterOperation()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoFlush, WrapCrossThreadPersistent(this),
                          WrapCrossThreadPersistent(resolver),
                          resolver_task_runner_));
  return resolver->Promise();
}

// static
void FileSystemSyncAccessHandle::DoFlush(
    CrossThreadPersistent<FileSystemSyncAccessHandle> access_handle,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";

  DCHECK(access_handle->file_delegate()->IsValid())
      << "file I/O operation queued after file closed";
  bool success = access_handle->file_delegate()->Flush();

  PostCrossThreadTask(*resolver_task_runner, FROM_HERE,
                      CrossThreadBindOnce(&FileSystemSyncAccessHandle::DidFlush,
                                          std::move(access_handle),
                                          std::move(resolver), success));
}

void FileSystemSyncAccessHandle::DidFlush(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  ExitOperation();
  if (!success) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "Flush failed"));
    return;
  }
  resolver->Resolve();
}

uint64_t FileSystemSyncAccessHandle::read(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  OperationScope scope(this);
  if (!scope.entered_operation()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "There is a pending operation on the access handle");
    return 0;
  }

  if (!file_delegate()->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  size_t read_size = buffer->byteLength();
  uint8_t* read_data = static_cast<uint8_t*>(buffer->BaseAddressMaybeShared());
  uint64_t file_offset = options->at();

  FileErrorOr<int> result =
      file_delegate()->Read(file_offset, {read_data, read_size});

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
  OperationScope scope(this);
  if (!scope.entered_operation()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "There is a pending operation on the access handle");
    return 0;
  }

  uint64_t file_offset = options->at();
  if (!file_delegate()->IsValid()) {
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
      file_delegate()->Write(file_offset, {write_data, write_size});
  if (result.is_error()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to write to the access handle");
    return 0;
  }

  return base::as_unsigned(result.value());
}

}  // namespace blink
