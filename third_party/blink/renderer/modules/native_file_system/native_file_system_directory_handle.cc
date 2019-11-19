// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_get_directory_options.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_get_file_options.h"
#include "third_party/blink/renderer/modules/native_file_system/file_system_remove_options.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_iterator.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {
// The name to use for the root directory of a sandboxed file system.
constexpr const char kSandboxRootDirectoryName[] = "";
}  // namespace

using mojom::blink::NativeFileSystemErrorPtr;

NativeFileSystemDirectoryHandle::NativeFileSystemDirectoryHandle(
    ExecutionContext* context,
    const String& name,
    mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle> mojo_ptr)
    : NativeFileSystemHandle(context, name),
      mojo_ptr_(std::move(mojo_ptr),
                context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {
  DCHECK(mojo_ptr_);
}

ScriptPromise NativeFileSystemDirectoryHandle::getFile(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetFileOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo_ptr_->GetFile(
      name, options->create(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, const String& name,
             NativeFileSystemErrorPtr result,
             mojo::PendingRemote<mojom::blink::NativeFileSystemFileHandle>
                 handle) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MakeGarbageCollected<NativeFileSystemFileHandle>(
                context, name, std::move(handle)));
          },
          WrapPersistent(resolver), name));

  return result;
}

ScriptPromise NativeFileSystemDirectoryHandle::getDirectory(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetDirectoryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (!mojo_ptr_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return result;
  }

  mojo_ptr_->GetDirectory(
      name, options->create(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, const String& name,
             NativeFileSystemErrorPtr result,
             mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>
                 handle) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(
                MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
                    context, name, std::move(handle)));
          },
          WrapPersistent(resolver), name));

  return result;
}

namespace {

void ReturnDataFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, info.Data());
}

}  // namespace

ScriptValue NativeFileSystemDirectoryHandle::getEntries(
    ScriptState* script_state) {
  auto* iterator = MakeGarbageCollected<NativeFileSystemDirectoryIterator>(
      this, ExecutionContext::From(script_state));
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!result
           ->Set(context, v8::Symbol::GetAsyncIterator(isolate),
                 v8::Function::New(context, &ReturnDataFunction,
                                   ToV8(iterator, script_state))
                     .ToLocalChecked())
           .ToChecked()) {
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), result);
}

ScriptPromise NativeFileSystemDirectoryHandle::removeEntry(
    ScriptState* script_state,
    const String& name,
    const FileSystemRemoveOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  if (!mojo_ptr_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError));
    return result;
  }

  mojo_ptr_->RemoveEntry(
      name, options->recursive(),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, NativeFileSystemErrorPtr result) {
            native_file_system_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(resolver)));

  return result;
}

// static
ScriptPromise NativeFileSystemDirectoryHandle::getSystemDirectory(
    ScriptState* script_state,
    const GetSystemDirectoryOptions* options) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessNativeFileSystem()) {
    if (context->GetSecurityContext().IsSandboxed(WebSandboxFlags::kOrigin)) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kSecurityError,
              "System directory access is denied because the context is "
              "sandboxed and lacks the 'allow-same-origin' flag."));
    } else {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kSecurityError,
                            "System directory access is denied."));
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  // TODO(mek): Cache mojo::Remote<mojom::blink::NativeFileSystemManager>
  // associated with an ExecutionContext, so we don't have to request a new one
  // for each operation, and can avoid code duplication between here and other
  // uses.
  mojo::Remote<mojom::blink::NativeFileSystemManager> manager;
  auto* provider = context->GetInterfaceProvider();
  if (!provider) {
    resolver->Reject(file_error::CreateDOMException(
        base::File::FILE_ERROR_INVALID_OPERATION));
    return result;
  }

  provider->GetInterface(manager.BindNewPipeAndPassReceiver());
  auto* raw_manager = manager.get();
  raw_manager->GetSandboxedFileSystem(WTF::Bind(
      [](ScriptPromiseResolver* resolver,
         mojo::Remote<mojom::blink::NativeFileSystemManager>,
         NativeFileSystemErrorPtr result,
         mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>
             handle) {
        ExecutionContext* context = resolver->GetExecutionContext();
        if (!context)
          return;
        if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
          native_file_system_error::Reject(resolver, *result);
          return;
        }
        resolver->Resolve(MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
            context, kSandboxRootDirectoryName, std::move(handle)));
      },
      WrapPersistent(resolver), std::move(manager)));

  return result;
}

mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
NativeFileSystemDirectoryHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> result;
  if (mojo_ptr_)
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void NativeFileSystemDirectoryHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemDirectoryHandle::RequestPermissionImpl(
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

void NativeFileSystemDirectoryHandle::ContextDestroyed(ExecutionContext*) {
  mojo_ptr_.reset();
}

}  // namespace blink
