// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/storage_manager_file_system_access.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_manager.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// The name to use for the root directory of a sandboxed file system.
constexpr const char kSandboxRootDirectoryName[] = "";

// Called with the result of browser-side permissions checks.
void OnGotAccessAllowed(
    ScriptPromiseResolver<FileSystemDirectoryHandle>* resolver,
    base::OnceCallback<void(ScriptPromiseResolver<FileSystemDirectoryHandle>*)>
        on_allowed,
    const mojom::blink::FileSystemAccessErrorPtr result) {
  if (!resolver->GetExecutionContext() ||
      !resolver->GetScriptState()->ContextIsValid()) {
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    auto* const isolate = resolver->GetScriptState()->GetIsolate();
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        isolate, DOMExceptionCode::kSecurityError, result->message));
    return;
  }

  std::move(on_allowed).Run(resolver);
}

}  // namespace

// static
ScriptPromise<FileSystemDirectoryHandle>
StorageManagerFileSystemAccess::getDirectory(ScriptState* script_state,
                                             const StorageManager& storage,
                                             ExceptionState& exception_state) {
  return CheckStorageAccessIsAllowed(
      script_state, exception_state,
      WTF::BindOnce([](ScriptPromiseResolver<FileSystemDirectoryHandle>*
                           resolver) {
        FileSystemAccessManager::From(resolver->GetExecutionContext())
            ->GetSandboxedFileSystem(WTF::BindOnce(
                &StorageManagerFileSystemAccess::DidGetSandboxedFileSystem,
                WrapPersistent(resolver)));
      }));
}

// static
ScriptPromise<FileSystemDirectoryHandle>
StorageManagerFileSystemAccess::CheckStorageAccessIsAllowed(
    ScriptState* script_state,
    ExceptionState& exception_state,
    base::OnceCallback<void(ScriptPromiseResolver<FileSystemDirectoryHandle>*)>
        on_allowed) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemDirectoryHandle>>(
          script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  CheckStorageAccessIsAllowed(
      ExecutionContext::From(script_state),
      WTF::BindOnce(&OnGotAccessAllowed, WrapPersistent(resolver),
                    std::move(on_allowed)));

  return result;
}

// static
void StorageManagerFileSystemAccess::CheckStorageAccessIsAllowed(
    ExecutionContext* context,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)> callback) {
  if (!context->GetSecurityOrigin()->CanAccessFileSystem()) {
    if (context->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
      std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kSecurityError,
          base::File::Error::FILE_ERROR_SECURITY,
          "Storage directory access is denied because the context is "
          "sandboxed and lacks the 'allow-same-origin' flag."));
      return;
    }
    std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
        mojom::blink::FileSystemAccessStatus::kSecurityError,
        base::File::Error::FILE_ERROR_SECURITY,
        "Storage directory access is denied."));
    return;
  }

  SECURITY_DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  auto storage_access_callback =
      [](base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)>
             inner_callback,
         bool is_allowed) {
        std::move(inner_callback)
            .Run(is_allowed
                     ? mojom::blink::FileSystemAccessError::New(
                           mojom::blink::FileSystemAccessStatus::kOk,
                           base::File::FILE_OK, "")
                     : mojom::blink::FileSystemAccessError::New(
                           mojom::blink::FileSystemAccessStatus::kSecurityError,
                           base::File::Error::FILE_ERROR_SECURITY,
                           "Storage directory access is denied."));
      };

  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kSecurityError,
          base::File::Error::FILE_ERROR_SECURITY,
          "Storage directory access is denied."));
      return;
    }
    frame->AllowStorageAccessAndNotify(
        WebContentSettingsClient::StorageType::kFileSystem,
        WTF::BindOnce(std::move(storage_access_callback), std::move(callback)));
    return;
  }

  WebContentSettingsClient* content_settings_client =
      To<WorkerGlobalScope>(context)->ContentSettingsClient();
  if (!content_settings_client) {
    std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
        mojom::blink::FileSystemAccessStatus::kOk, base::File::FILE_OK, ""));
    return;
  }
  content_settings_client->AllowStorageAccess(
      WebContentSettingsClient::StorageType::kFileSystem,
      WTF::BindOnce(std::move(storage_access_callback), std::move(callback)));
}

// static
void StorageManagerFileSystemAccess::DidGetSandboxedFileSystem(
    ScriptPromiseResolver<FileSystemDirectoryHandle>* resolver,
    mojom::blink::FileSystemAccessErrorPtr result,
    mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle> handle) {
  ExecutionContext* context = resolver->GetExecutionContext();
  if (!context)
    return;
  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    file_system_access_error::Reject(resolver, *result);
    return;
  }
  resolver->Resolve(MakeGarbageCollected<FileSystemDirectoryHandle>(
      context, kSandboxRootDirectoryName, std::move(handle)));
}

void StorageManagerFileSystemAccess::DidGetSandboxedFileSystemForDevtools(
    ExecutionContext* context,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                            FileSystemDirectoryHandle*)> callback,
    mojom::blink::FileSystemAccessErrorPtr result,
    mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle> handle) {
  if (!context) {
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    std::move(callback).Run(std::move(result), nullptr);
    return;
  }

  std::move(callback).Run(
      std::move(result),
      MakeGarbageCollected<FileSystemDirectoryHandle>(
          context, kSandboxRootDirectoryName, std::move(handle)));
}

}  // namespace blink
