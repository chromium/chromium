// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_create_writable_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_writable_file_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::FileSystemAccessErrorPtr;

FileSystemFileHandle::FileSystemFileHandle(
    ExecutionContext* context,
    const String& name,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle> mojo_ptr)
    : FileSystemHandle(context, name), mojo_ptr_(context) {
  mojo_ptr_.Bind(std::move(mojo_ptr),
                 context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(mojo_ptr_.is_bound());
}

ScriptPromise FileSystemFileHandle::createWritable(
    ScriptState* script_state,
    const FileSystemCreateWritableOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->CreateFileWriter(
      options->keepExistingData(), options->autoClose(),
      WTF::BindOnce(
          [](FileSystemFileHandle*, ScriptPromiseResolver* resolver,
             mojom::blink::FileSystemAccessErrorPtr result,
             mojo::PendingRemote<mojom::blink::FileSystemAccessFileWriter>
                 writer) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            ScriptState* script_state = resolver->GetScriptState();
            if (!script_state)
              return;
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }

            resolver->Resolve(FileSystemWritableFileStream::Create(
                script_state, std::move(writer)));
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemFileHandle::getFile(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->AsBlob(WTF::BindOnce(
      [](FileSystemFileHandle*, ScriptPromiseResolver* resolver,
         const String& name, FileSystemAccessErrorPtr result,
         const base::File::Info& info,
         const scoped_refptr<BlobDataHandle>& blob) {
        // Keep `this` alive so the handle will not be garbage-collected
        // before the promise is resolved.
        if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
          file_system_access_error::Reject(resolver, *result);
          return;
        }
        resolver->Resolve(MakeGarbageCollected<File>(
            name, NullableTimeToOptionalTime(info.last_modified), blob));
      },
      WrapPersistent(this), WrapPersistent(resolver), name()));

  return result;
}

ScriptPromise FileSystemFileHandle::createSyncAccessHandle(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // TODO(fivedots): Check if storage access is allowed.
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->OpenAccessHandle(WTF::BindOnce(
      [](FileSystemFileHandle*, ScriptPromiseResolver* resolver,
         FileSystemAccessErrorPtr result,
         mojom::blink::FileSystemAccessAccessHandleFilePtr file,
         mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
             access_handle_remote) {
        // Keep `this` alive so the handle will not be garbage-collected
        // before the promise is resolved.
        if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
          file_system_access_error::Reject(resolver, *result);
          return;
        }
        DCHECK(!file.is_null());
        DCHECK(access_handle_remote.is_valid());

        ExecutionContext* context = resolver->GetExecutionContext();
        if (!context)
          return;

        FileSystemAccessFileDelegate* file_delegate = nullptr;
        if (file->is_regular_file()) {
          mojom::blink::FileSystemAccessRegularFilePtr regular_file =
              std::move(file->get_regular_file());
          file_delegate = FileSystemAccessFileDelegate::Create(
              context, std::move(regular_file));
        } else if (file->is_incognito_file_delegate()) {
          file_delegate = FileSystemAccessFileDelegate::CreateForIncognito(
              context, std::move(file->get_incognito_file_delegate()));
        }

        if (!file_delegate || !file_delegate->IsValid()) {
          file_system_access_error::Reject(
              resolver,
              *mojom::blink::FileSystemAccessError::New(
                  mojom::blink::FileSystemAccessStatus::kFileError,
                  base::File::Error::FILE_ERROR_FAILED, "File not valid"));
          return;
        }
        resolver->Resolve(MakeGarbageCollected<FileSystemSyncAccessHandle>(
            context, std::move(file_delegate),
            std::move(access_handle_remote)));
      },
      WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>
FileSystemFileHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> result;
  if (mojo_ptr_.is_bound())
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void FileSystemFileHandle::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  FileSystemHandle::Trace(visitor);
}

void FileSystemFileHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void FileSystemFileHandle::RequestPermissionImpl(
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

void FileSystemFileHandle::MoveImpl(
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

void FileSystemFileHandle::RemoveImpl(
    const FileSystemRemoveOptions* options,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
        mojom::blink::FileSystemAccessStatus::kInvalidState,
        base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"));
    return;
  }

  mojo_ptr_->Remove(std::move(callback));
}

void FileSystemFileHandle::IsSameEntryImpl(
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

  mojo_ptr_->IsSameEntry(std::move(other), std::move(callback));
}

void FileSystemFileHandle::GetUniqueIdImpl(
    base::OnceCallback<void(const WTF::String&)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run("");
    return;
  }
  mojo_ptr_->GetUniqueId(std::move(callback));
}

}  // namespace blink
