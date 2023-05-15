// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_get_directory_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_get_file_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_remove_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_iterator.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::FileSystemAccessErrorPtr;

FileSystemDirectoryHandle::FileSystemDirectoryHandle(
    ExecutionContext* context,
    const String& name,
    mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle> mojo_ptr)
    : FileSystemHandle(context, name), mojo_ptr_(context) {
  mojo_ptr_.Bind(std::move(mojo_ptr),
                 context->GetTaskRunner(TaskType::kStorage));
  DCHECK(mojo_ptr_.is_bound());
}

FileSystemDirectoryIterator* FileSystemDirectoryHandle::entries(
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "entries() may not be called in a detached window");
    return nullptr;
  }

  return MakeGarbageCollected<FileSystemDirectoryIterator>(
      this, FileSystemDirectoryIterator::Mode::kKeyValue, execution_context);
}

FileSystemDirectoryIterator* FileSystemDirectoryHandle::keys(
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "keys() may not be called in a detached window");
    return nullptr;
  }

  return MakeGarbageCollected<FileSystemDirectoryIterator>(
      this, FileSystemDirectoryIterator::Mode::kKey, execution_context);
}

FileSystemDirectoryIterator* FileSystemDirectoryHandle::values(
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "values() may not be called in a detached window");
    return nullptr;
  }

  return MakeGarbageCollected<FileSystemDirectoryIterator>(
      this, FileSystemDirectoryIterator::Mode::kValue, execution_context);
}

ScriptPromise FileSystemDirectoryHandle::getFileHandle(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetFileOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->GetFile(
      name, options->create(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*, ScriptPromiseResolver* resolver,
             const String& name, FileSystemAccessErrorPtr result,
             mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle>
                 handle) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MakeGarbageCollected<FileSystemFileHandle>(
                context, name, std::move(handle)));
          },
          WrapPersistent(this), WrapPersistent(resolver), name));

  return result;
}

ScriptPromise FileSystemDirectoryHandle::getDirectoryHandle(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetDirectoryOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->GetDirectory(
      name, options->create(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*, ScriptPromiseResolver* resolver,
             const String& name, FileSystemAccessErrorPtr result,
             mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle>
                 handle) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MakeGarbageCollected<FileSystemDirectoryHandle>(
                context, name, std::move(handle)));
          },
          WrapPersistent(this), WrapPersistent(resolver), name));

  return result;
}

ScriptPromise FileSystemDirectoryHandle::removeEntry(
    ScriptState* script_state,
    const String& name,
    const FileSystemRemoveOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->RemoveEntry(
      name, options->recursive(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*, ScriptPromiseResolver* resolver,
             FileSystemAccessErrorPtr result) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            file_system_access_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemDirectoryHandle::resolve(
    ScriptState* script_state,
    FileSystemHandle* possible_child,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->Resolve(
      possible_child->Transfer(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*, ScriptPromiseResolver* resolver,
             FileSystemAccessErrorPtr result,
             const absl::optional<Vector<String>>& path) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            if (!path.has_value()) {
              resolver->Resolve(static_cast<ScriptWrappable*>(nullptr));
              return;
            }
            resolver->Resolve(*path);
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>
FileSystemDirectoryHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> result;
  if (mojo_ptr_.is_bound())
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void FileSystemDirectoryHandle::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  FileSystemHandle::Trace(visitor);
}

void FileSystemDirectoryHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void FileSystemDirectoryHandle::RequestPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                            mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::FileSystemAccessError::New(
            mojom::blink::FileSystemAccessStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        mojom::blink::PermissionStatus::DENIED);
    return;
  }

  mojo_ptr_->RequestPermission(writable, std::move(callback));
}

void FileSystemDirectoryHandle::MoveImpl(
    mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> dest,
    const String& new_entry_name,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
        mojom::blink::FileSystemAccessStatus::kInvalidState,
        base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"));
    return;
  }

  if (dest.is_valid()) {
    mojo_ptr_->Move(std::move(dest), new_entry_name, std::move(callback));
  } else {
    mojo_ptr_->Rename(new_entry_name, std::move(callback));
  }
}

void FileSystemDirectoryHandle::RemoveImpl(
    const FileSystemRemoveOptions* options,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
        mojom::blink::FileSystemAccessStatus::kInvalidState,
        base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"));
    return;
  }

  mojo_ptr_->Remove(options->recursive(), std::move(callback));
}

void FileSystemDirectoryHandle::IsSameEntryImpl(
    mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> other,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr, bool)>
        callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::FileSystemAccessError::New(
            mojom::blink::FileSystemAccessStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        false);
    return;
  }

  mojo_ptr_->Resolve(
      std::move(other),
      WTF::BindOnce(
          [](base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                                     bool)> callback,
             FileSystemAccessErrorPtr result,
             const absl::optional<Vector<String>>& path) {
            std::move(callback).Run(std::move(result),
                                    path.has_value() && path->empty());
          },
          std::move(callback)));
}

void FileSystemDirectoryHandle::GetUniqueIdImpl(
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                            const WTF::String&)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::FileSystemAccessError::New(
            mojom::blink::FileSystemAccessStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        "");
    return;
  }
  mojo_ptr_->GetUniqueId(std::move(callback));
}

}  // namespace blink
