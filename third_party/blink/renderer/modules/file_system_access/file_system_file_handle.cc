// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_create_sync_access_handle_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_create_writable_options.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_writable_file_stream.h"
#include "third_party/blink/renderer/modules/file_system_access/storage_manager_file_system_access.h"
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
                 context->GetTaskRunner(TaskType::kStorage));
  DCHECK(mojo_ptr_.is_bound());
}

ScriptPromise<FileSystemWritableFileStream>
FileSystemFileHandle::createWritable(
    ScriptState* script_state,
    const FileSystemCreateWritableOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemWritableFileStream>>(
          script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  mojom::blink::FileSystemAccessWritableFileStreamLockMode lock_mode;

  switch (options->mode().AsEnum()) {
    case V8FileSystemWritableFileStreamMode::Enum::kExclusive:
      lock_mode =
          mojom::blink::FileSystemAccessWritableFileStreamLockMode::kExclusive;
      break;
    case V8FileSystemWritableFileStreamMode::Enum::kSiloed:
      lock_mode =
          mojom::blink::FileSystemAccessWritableFileStreamLockMode::kSiloed;
      break;
  }

  mojo_ptr_->CreateFileWriter(
      options->keepExistingData(), options->autoClose(), lock_mode,
      WTF::BindOnce(
          [](FileSystemFileHandle*,
             ScriptPromiseResolver<FileSystemWritableFileStream>* resolver,
             V8FileSystemWritableFileStreamMode lock_mode,
             mojom::blink::FileSystemAccessErrorPtr result,
             mojo::PendingRemote<mojom::blink::FileSystemAccessFileWriter>
                 writer) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            ScriptState* script_state = resolver->GetScriptState();
            if (!script_state) {
              return;
            }
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }

            resolver->Resolve(FileSystemWritableFileStream::Create(
                script_state, std::move(writer), lock_mode));
          },
          WrapPersistent(this), WrapPersistent(resolver), options->mode()));

  return result;
}

ScriptPromise<File> FileSystemFileHandle::getFile(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<File>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  mojo_ptr_->AsBlob(WTF::BindOnce(
      [](FileSystemFileHandle*, ScriptPromiseResolver<File>* resolver,
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

ScriptPromise<FileSystemSyncAccessHandle>
FileSystemFileHandle::createSyncAccessHandle(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return createSyncAccessHandle(
      script_state, FileSystemCreateSyncAccessHandleOptions::Create(),
      exception_state);
}

ScriptPromise<FileSystemSyncAccessHandle>
FileSystemFileHandle::createSyncAccessHandle(
    ScriptState* script_state,
    const FileSystemCreateSyncAccessHandleOptions* options,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemSyncAccessHandle>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  auto on_allowed_callback =
      WTF::BindOnce(&FileSystemFileHandle::CreateSyncAccessHandleImpl,
                    WrapWeakPersistent(this), WrapPersistent(options),
                    WrapPersistent(resolver));

  auto on_got_storage_access_status_cb =
      WTF::BindOnce(&FileSystemFileHandle::OnGotFileSystemStorageAccessStatus,
                    WrapWeakPersistent(this), WrapPersistent(resolver),
                    std::move(on_allowed_callback));

  if (storage_access_status_.has_value()) {
    std::move(on_got_storage_access_status_cb)
        .Run(mojom::blink::FileSystemAccessError::New(
            /*status=*/get<0>(storage_access_status_.value()),
            /*file_error=*/get<1>(storage_access_status_.value()),
            /*message=*/get<2>(storage_access_status_.value())));
  } else {
    StorageManagerFileSystemAccess::CheckStorageAccessIsAllowed(
        ExecutionContext::From(script_state),
        std::move(on_got_storage_access_status_cb));
  }

  return promise;
}

void FileSystemFileHandle::CreateSyncAccessHandleImpl(
    const FileSystemCreateSyncAccessHandleOptions* options,
    ScriptPromiseResolver<FileSystemSyncAccessHandle>* resolver) {
  if (!mojo_ptr_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError, "");
    return;
  }

  mojom::blink::FileSystemAccessAccessHandleLockMode lock_mode;

  // This assertion protects against the IDL enum changing without updating the
  // corresponding mojom interface, or vice versa.
  //
  static_assert(
      V8FileSystemSyncAccessHandleMode::kEnumSize ==
          static_cast<size_t>(
              mojom::blink::FileSystemAccessAccessHandleLockMode::kMaxValue)
              // This offset of 1 accounts for the zero-indexing of the mojom
              // enum values.
              + 1
              // TODO(crbug/1513463): This offset of 1 accounts for the
              // "in-place" option. This should be removed.
              + 1,
      "the number of values in the FileSystemAccessAccessHandleLockMode mojom "
      "enum must match the number of values in the "
      "FileSystemSyncAccessHandleMode blink enum");

  switch (options->mode().AsEnum()) {
    // TODO(crbug/1513463): "in-place" acts as an alternative to "readwrite".
    // This is for backwards compatibility and should be removed.
    case V8FileSystemSyncAccessHandleMode::Enum::kInPlace:
    case V8FileSystemSyncAccessHandleMode::Enum::kReadwrite:
      lock_mode =
          mojom::blink::FileSystemAccessAccessHandleLockMode::kReadwrite;
      break;
    case V8FileSystemSyncAccessHandleMode::Enum::kReadOnly:
      lock_mode = mojom::blink::FileSystemAccessAccessHandleLockMode::kReadOnly;
      break;
    case V8FileSystemSyncAccessHandleMode::Enum::kReadwriteUnsafe:
      lock_mode =
          mojom::blink::FileSystemAccessAccessHandleLockMode::kReadwriteUnsafe;
      break;
  }

  mojo_ptr_->OpenAccessHandle(
      lock_mode,
      WTF::BindOnce(
          [](FileSystemFileHandle*,
             ScriptPromiseResolver<FileSystemSyncAccessHandle>* resolver,
             V8FileSystemSyncAccessHandleMode lock_mode,
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
            if (!context) {
              return;
            }

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
                std::move(access_handle_remote), std::move(lock_mode)));
          },
          WrapPersistent(this), WrapPersistent(resolver), options->mode()));
}

mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>
FileSystemFileHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> result;
  if (mojo_ptr_.is_bound()) {
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  }
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

void FileSystemFileHandle::GetCloudIdentifiersImpl(
    base::OnceCallback<void(
        mojom::blink::FileSystemAccessErrorPtr,
        Vector<mojom::blink::FileSystemAccessCloudIdentifierPtr>)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::FileSystemAccessError::New(
            mojom::blink::FileSystemAccessStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        {});
    return;
  }
  mojo_ptr_->GetCloudIdentifiers(std::move(callback));
}

void FileSystemFileHandle::OnGotFileSystemStorageAccessStatus(
    ScriptPromiseResolver<FileSystemSyncAccessHandle>* resolver,
    base::OnceClosure on_allowed_callback,
    mojom::blink::FileSystemAccessErrorPtr result) {
  if (!resolver->GetExecutionContext() ||
      !resolver->GetScriptState()->ContextIsValid()) {
    return;
  }

  CHECK(result);
  if (storage_access_status_.has_value()) {
    CHECK_EQ(/*status=*/get<0>(storage_access_status_.value()), result->status);
    CHECK_EQ(/*file_error=*/get<1>(storage_access_status_.value()),
             result->file_error);
    CHECK_EQ(/*message=*/get<2>(storage_access_status_.value()),
             result->message);
  } else {
    storage_access_status_ =
        std::make_tuple(result->status, result->file_error, result->message);
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    auto* const isolate = resolver->GetScriptState()->GetIsolate();
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        isolate, DOMExceptionCode::kSecurityError, result->message));
    return;
  }

  std::move(on_allowed_callback).Run();
}

}  // namespace blink
