// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"

#include "base/files/file_error_or.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
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

void FileSystemSyncAccessHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(file_delegate_);
  visitor->Trace(access_handle_remote_);
  visitor->Trace(queued_close_resolver_);
}

ScriptValue FileSystemSyncAccessHandle::close(ScriptState* script_state) {
  if (is_all_sync_interface_enabled_) {
    CloseSync(script_state);
    return ScriptValue::From(script_state, ToV8UndefinedGenerator());
  } else {
    return ScriptValue::From(script_state, CloseAsync(script_state));
  }
}

void FileSystemSyncAccessHandle::CloseSync(ScriptState* script_state) {
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

ScriptPromise FileSystemSyncAccessHandle::CloseAsync(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  if (is_closed_ || !access_handle_remote_.is_bound()) {
    // close() is idempotent.
    resolver->Resolve();
    return promise;
  }

  is_closed_ = true;

  DCHECK(!queued_close_resolver_) << "Close logic kicked off twice";
  queued_close_resolver_ = resolver;

  if (!io_pending_) {
    // Pretend that a close() promise was queued behind an I/O operation, and
    // the operation just finished. This is less logic than handling the
    // non-queued case separately.
    DispatchQueuedClose();
  }

  return promise;
}

void FileSystemSyncAccessHandle::DispatchQueuedClose() {
  DCHECK(!io_pending_)
      << "Dispatching close() concurrently with other I/O operations is racy";

  if (!queued_close_resolver_)
    return;

  DCHECK(is_closed_) << "close() resolver queued without setting closed_";
  ScriptPromiseResolver* resolver = queued_close_resolver_;
  queued_close_resolver_ = nullptr;

  // Access file delegate directly rather than through accessor method, which
  // checks `io_pending_`.
  DCHECK(file_delegate_->IsValid())
      << "file I/O operation queued after file closed";

  file_delegate_->CloseAsync(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver,
         FileSystemSyncAccessHandle* access_handle) {
        ScriptState* script_state = resolver->GetScriptState();
        if (!script_state->ContextIsValid())
          return;
        ScriptState::Scope scope(script_state);

        access_handle->access_handle_remote_->Close(WTF::BindOnce(
            [](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
            WrapPersistent(resolver)));
      },
      WrapPersistent(resolver), WrapPersistent(this)));
}

ScriptValue FileSystemSyncAccessHandle::flush(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  if (is_all_sync_interface_enabled_) {
    FlushSync(script_state, exception_state);
    return ScriptValue::From(script_state, ToV8UndefinedGenerator());
  } else {
    return ScriptValue::From(script_state, FlushAsync(script_state));
  }
}

void FileSystemSyncAccessHandle::FlushSync(ScriptState* script_state,
                                           ExceptionState& exception_state) {
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

ScriptPromise FileSystemSyncAccessHandle::FlushAsync(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (is_closed_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "The file was already closed"));
    return result;
  }

  if (!EnterOperation()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file"));
    return result;
  }

  DCHECK(file_delegate()->IsValid())
      << "file I/O operation queued after file closed";

  file_delegate()->FlushAsync(WTF::BindOnce(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver,
         FileSystemSyncAccessHandle* access_handle, bool success) {
        ScriptState* script_state = resolver->GetScriptState();
        if (!script_state->ContextIsValid())
          return;
        ScriptState::Scope scope(script_state);

        access_handle->ExitOperation();
        if (!success) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
              "flush failed"));
          return;
        }
        resolver->Resolve();
      },
      WrapPersistent(resolver), WrapPersistent(this))));

  return result;
}

ScriptValue FileSystemSyncAccessHandle::getSize(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (is_all_sync_interface_enabled_) {
    return ScriptValue::From(script_state,
                             GetSizeSync(script_state, exception_state));
  } else {
    return ScriptValue::From(script_state, GetSizeAsync(script_state));
  }
}

uint64_t FileSystemSyncAccessHandle::GetSizeSync(
    ScriptState* script_state,
    ExceptionState& exception_state) {
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

ScriptPromise FileSystemSyncAccessHandle::GetSizeAsync(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (is_closed_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "The file was already closed"));
    return result;
  }

  if (!EnterOperation()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file"));
    return result;
  }

  DCHECK(file_delegate()->IsValid())
      << "file I/O operation queued after file closed";

  file_delegate()->GetLengthAsync(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver,
         FileSystemSyncAccessHandle* access_handle,
         base::FileErrorOr<int64_t> error_or_length) {
        ScriptState* script_state = resolver->GetScriptState();
        if (!script_state->ContextIsValid())
          return;
        ScriptState::Scope scope(script_state);

        access_handle->ExitOperation();
        if (!error_or_length.has_value()) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
              "getSize failed"));
          return;
        }
        resolver->Resolve(error_or_length.value());
      },
      WrapPersistent(resolver), WrapPersistent(this)));

  return result;
}

ScriptValue FileSystemSyncAccessHandle::truncate(
    ScriptState* script_state,
    uint64_t size,
    ExceptionState& exception_state) {
  if (is_all_sync_interface_enabled_) {
    TruncateSync(script_state, size, exception_state);
    return ScriptValue::From(script_state, ToV8UndefinedGenerator());
  } else {
    return ScriptValue::From(script_state, TruncateAsync(script_state, size));
  }
}

void FileSystemSyncAccessHandle::TruncateSync(ScriptState* script_state,
                                              uint64_t size,
                                              ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return;
  }

  DCHECK(file_delegate_->IsValid())
      << "file delgate invalidated before truncate";

  if (!base::CheckedNumeric<int64_t>(size).IsValid() ||
      !base::CheckedNumeric<uint64_t>(size).IsValid()) {
    exception_state.ThrowTypeError("Cannot truncate file to given length");
    return;
  }

  base::FileErrorOr<bool> result = file_delegate()->SetLength(size);
  if (result.has_value())
    return;

  base::File::Error file_error = result.error();
  if (file_error == base::File::FILE_ERROR_NO_SPACE) {
    exception_state.ThrowDOMException(DOMExceptionCode::kQuotaExceededError,
                                      "No space available for this operation");
    return;
  }
  if (file_error != base::File::FILE_OK) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "truncate failed");
    return;
  }
}

ScriptPromise FileSystemSyncAccessHandle::TruncateAsync(
    ScriptState* script_state,
    uint64_t size) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (is_closed_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "The file was already closed"));
    return result;
  }

  if (!base::CheckedNumeric<int64_t>(size).IsValid() ||
      !base::CheckedNumeric<uint64_t>(size).IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Cannot truncate file to given length"));
    return result;
  }

  if (!EnterOperation()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file"));
    return result;
  }

  DCHECK(file_delegate()->IsValid())
      << "file I/O operation queued after file closed";

  file_delegate()->SetLengthAsync(
      size,
      WTF::BindOnce(
          [](ScriptPromiseResolver* resolver,
             FileSystemSyncAccessHandle* access_handle,
             base::File::Error file_error) {
            ScriptState* script_state = resolver->GetScriptState();
            if (!script_state->ContextIsValid())
              return;
            ScriptState::Scope scope(script_state);

            access_handle->ExitOperation();
            if (file_error == base::File::FILE_ERROR_NO_SPACE) {
              resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                  script_state->GetIsolate(),
                  DOMExceptionCode::kQuotaExceededError,
                  "No space available for this operation"));
              return;
            }
            if (file_error != base::File::FILE_OK) {
              resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                  script_state->GetIsolate(),
                  DOMExceptionCode::kInvalidStateError, "truncate failed"));
              return;
            }
            resolver->Resolve(true);
          },
          WrapPersistent(resolver), WrapPersistent(this)));

  return result;
}

uint64_t FileSystemSyncAccessHandle::read(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  if (is_all_sync_interface_enabled_) {
    return DoRead(buffer, options, exception_state);
  } else {
    // TODO(crbug.com/1338340): OperationScope is only used for async methods.
    // Remove once we migrate all methods to be sync.
    OperationScope scope(this);
    if (!scope.entered_operation()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "There is a pending operation on the access handle");
      return 0;
    }
    return DoRead(buffer, options, exception_state);
  }
}

uint64_t FileSystemSyncAccessHandle::DoRead(
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
  uint64_t file_offset = options->at();
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
  return base::as_unsigned(result.value());
}

uint64_t FileSystemSyncAccessHandle::write(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  if (is_all_sync_interface_enabled_) {
    return DoWrite(buffer, options, exception_state);
  } else {
    // TODO(crbug.com/1338340): OperationScope is only used for async methods.
    // Remove once we migrate all methods to be sync.
    OperationScope scope(this);
    if (!scope.entered_operation()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "There is a pending operation on the access handle");
      return 0;
    }
    return DoWrite(buffer, options, exception_state);
  }
}

uint64_t FileSystemSyncAccessHandle::DoWrite(
    MaybeShared<DOMArrayBufferView> buffer,
    FileSystemReadWriteOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file_delegate()->IsValid() || is_closed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The access handle was already closed");
    return 0;
  }

  uint64_t file_offset = options->at();
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
      return 0;
    }
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to write to the access handle");
    return 0;
  }

  return base::as_unsigned(result.value());
}

}  // namespace blink
