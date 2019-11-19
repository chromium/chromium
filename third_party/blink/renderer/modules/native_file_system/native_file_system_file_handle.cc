// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"

#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_writer.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_create_writer_options.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_writer.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
using mojom::blink::NativeFileSystemErrorPtr;

NativeFileSystemFileHandle::NativeFileSystemFileHandle(
    ExecutionContext* context,
    const String& name,
    mojo::PendingRemote<mojom::blink::NativeFileSystemFileHandle> mojo_ptr)
    : NativeFileSystemHandle(context, name),
      mojo_ptr_(std::move(mojo_ptr),
                context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {
  DCHECK(mojo_ptr_);
}

ScriptPromise NativeFileSystemFileHandle::createWriter(
    ScriptState* script_state,
    const FileSystemCreateWriterOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (!mojo_ptr_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return result;
  }

  mojo_ptr_->CreateFileWriter(
      options->keepExistingData(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             mojom::blink::NativeFileSystemErrorPtr result,
             mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter>
                 writer) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MakeGarbageCollected<NativeFileSystemWriter>(
                context, std::move(writer)));
          },
          WrapPersistent(resolver)));

  return result;
}

ScriptPromise NativeFileSystemFileHandle::getFile(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (!mojo_ptr_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return result;
  }

  mojo_ptr_->AsBlob(WTF::Bind(
      [](ScriptPromiseResolver* resolver, const String& name,
         NativeFileSystemErrorPtr result, const base::File::Info& info,
         const scoped_refptr<BlobDataHandle>& blob) {
        if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
          native_file_system_error::Reject(resolver, *result);
          return;
        }
        double last_modified = info.last_modified.is_null()
                                   ? InvalidFileTime()
                                   : info.last_modified.ToJsTime();
        resolver->Resolve(File::Create(name, last_modified, blob));
      },
      WrapPersistent(resolver), name()));

  return result;
}

mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
NativeFileSystemFileHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> result;
  if (mojo_ptr_)
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void NativeFileSystemFileHandle::ContextDestroyed(ExecutionContext*) {
  mojo_ptr_.reset();
}

void NativeFileSystemFileHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemFileHandle::RequestPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr,
                            mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_) {
    std::move(callback).Run(
        mojom::blink::NativeFileSystemError::New(
            mojom::blink::NativeFileSystemStatus::kInvalidState,
            base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"),
        mojom::blink::PermissionStatus::DENIED);
    return;
  }

  mojo_ptr_->RequestPermission(writable, std::move(callback));
}

}  // namespace blink
