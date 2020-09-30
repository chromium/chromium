// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/native_file_system_file_handle.h"

#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_writer.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_create_writer_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_writable_file_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
using mojom::blink::NativeFileSystemErrorPtr;

NativeFileSystemFileHandle::NativeFileSystemFileHandle(
    ExecutionContext* context,
    const String& name,
    mojo::PendingRemote<mojom::blink::NativeFileSystemFileHandle> mojo_ptr)
    : NativeFileSystemHandle(context, name), mojo_ptr_(context) {
  mojo_ptr_.Bind(std::move(mojo_ptr),
                 context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(mojo_ptr_.is_bound());
}

ScriptPromise NativeFileSystemFileHandle::createWritable(
    ScriptState* script_state,
    const FileSystemCreateWriterOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->CreateFileWriter(
      options->keepExistingData(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             mojom::blink::NativeFileSystemErrorPtr result,
             mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter>
                 writer) {
            ScriptState* script_state = resolver->GetScriptState();
            if (!script_state)
              return;
            if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver, *result);
              return;
            }

            resolver->Resolve(NativeFileSystemWritableFileStream::Create(
                script_state, std::move(writer)));
          },
          WrapPersistent(resolver)));

  return result;
}

ScriptPromise NativeFileSystemFileHandle::getFile(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->AsBlob(WTF::Bind(
      [](ScriptPromiseResolver* resolver, const String& name,
         NativeFileSystemErrorPtr result, const base::File::Info& info,
         const scoped_refptr<BlobDataHandle>& blob) {
        if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
          native_file_system_error::Reject(resolver, *result);
          return;
        }
        resolver->Resolve(MakeGarbageCollected<File>(
            name, NullableTimeToOptionalTime(info.last_modified), blob));
      },
      WrapPersistent(resolver), name()));

  return result;
}

mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
NativeFileSystemFileHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> result;
  if (mojo_ptr_.is_bound())
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void NativeFileSystemFileHandle::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  NativeFileSystemHandle::Trace(visitor);
}

void NativeFileSystemFileHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemFileHandle::RequestPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr,
                            mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::NativeFileSystemError::New(
            mojom::blink::NativeFileSystemStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        mojom::blink::PermissionStatus::DENIED);
    return;
  }

  mojo_ptr_->RequestPermission(writable, std::move(callback));
}

void NativeFileSystemFileHandle::IsSameEntryImpl(
    mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> other,
    base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr, bool)>
        callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(
        mojom::blink::NativeFileSystemError::New(
            mojom::blink::NativeFileSystemStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        false);
    return;
  }

  mojo_ptr_->IsSameEntry(std::move(other), std::move(callback));
}

}  // namespace blink
