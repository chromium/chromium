/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/modules/filesystem/worker_global_scope_file_system.h"

#include <memory>

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/filesystem/async_callback_helper.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/local_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/sync_callback_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void WorkerGlobalScopeFileSystem::webkitRequestFileSystem(
    WorkerGlobalScope& worker,
    int type,
    int64_t size,
    V8FileSystemCallback* success_callback,
    V8ErrorCallback* error_callback) {
  ExecutionContext* secure_context = worker.GetExecutionContext();
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem()) {
    DOMFileSystem::ReportError(&worker, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_SECURITY);
    return;
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  mojom::blink::FileSystemType file_system_type =
      static_cast<mojom::blink::FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    DOMFileSystem::ReportError(&worker, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<DOMFileSystem>(success_callback);

  LocalFileSystem::From(worker)->RequestFileSystem(
      file_system_type, size,
      std::make_unique<FileSystemCallbacks>(std::move(success_callback_wrapper),
                                            std::move(error_callback_wrapper),
                                            &worker, file_system_type),
      LocalFileSystem::kAsynchronous);
}

DOMFileSystemSync* WorkerGlobalScopeFileSystem::webkitRequestFileSystemSync(
    WorkerGlobalScope& worker,
    int type,
    int64_t size,
    ExceptionState& exception_state) {
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem()) {
    exception_state.ThrowSecurityError(file_error::kSecurityErrorMessage);
    return nullptr;
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  mojom::blink::FileSystemType file_system_type =
      static_cast<mojom::blink::FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "the type must be kTemporary or kPersistent.");
    return nullptr;
  }

  auto* sync_helper = MakeGarbageCollected<FileSystemCallbacksSyncHelper>();

  auto success_callback_wrapper =
      WTF::BindOnce(&FileSystemCallbacksSyncHelper::OnSuccess,
                    WrapPersistentIfNeeded(sync_helper));
  auto error_callback_wrapper =
      WTF::BindOnce(&FileSystemCallbacksSyncHelper::OnError,
                    WrapPersistentIfNeeded(sync_helper));

  auto callbacks = std::make_unique<FileSystemCallbacks>(
      std::move(success_callback_wrapper), std::move(error_callback_wrapper),
      &worker, file_system_type);

  LocalFileSystem::From(worker)->RequestFileSystem(
      file_system_type, size, std::move(callbacks),
      LocalFileSystem::kSynchronous);
  DOMFileSystem* file_system = sync_helper->GetResultOrThrow(exception_state);
  return file_system ? MakeGarbageCollected<DOMFileSystemSync>(file_system)
                     : nullptr;
}

void WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemURL(
    WorkerGlobalScope& worker,
    const String& url,
    V8EntryCallback* success_callback,
    V8ErrorCallback* error_callback) {
  KURL completed_url = worker.CompleteURL(url);
  ExecutionContext* secure_context = worker.GetExecutionContext();
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem() ||
      !secure_context->GetSecurityOrigin()->CanRequest(completed_url)) {
    DOMFileSystem::ReportError(&worker, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_SECURITY);
    return;
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  if (!completed_url.IsValid()) {
    DOMFileSystem::ReportError(&worker, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<Entry>(success_callback);

  LocalFileSystem::From(worker)->ResolveURL(
      completed_url,
      std::make_unique<ResolveURICallbacks>(std::move(success_callback_wrapper),
                                            std::move(error_callback_wrapper),
                                            &worker),
      LocalFileSystem::kAsynchronous);
}

EntrySync* WorkerGlobalScopeFileSystem::webkitResolveLocalFileSystemSyncURL(
    WorkerGlobalScope& worker,
    const String& url,
    ExceptionState& exception_state) {
  KURL completed_url = worker.CompleteURL(url);
  ExecutionContext* secure_context = worker.GetExecutionContext();
  if (!secure_context->GetSecurityOrigin()->CanAccessFileSystem() ||
      !secure_context->GetSecurityOrigin()->CanRequest(completed_url)) {
    exception_state.ThrowSecurityError(file_error::kSecurityErrorMessage);
    return nullptr;
  } else if (secure_context->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(secure_context, WebFeature::kFileAccessedFileSystem);
  }

  if (!completed_url.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kEncodingError,
                                      "the URL '" + url + "' is invalid.");
    return nullptr;
  }

  auto* sync_helper = MakeGarbageCollected<EntryCallbacksSyncHelper>();

  auto success_callback_wrapper =
      WTF::BindOnce(&EntryCallbacksSyncHelper::OnSuccess,
                    WrapPersistentIfNeeded(sync_helper));
  auto error_callback_wrapper = WTF::BindOnce(
      &EntryCallbacksSyncHelper::OnError, WrapPersistentIfNeeded(sync_helper));

  std::unique_ptr<ResolveURICallbacks> callbacks =
      std::make_unique<ResolveURICallbacks>(std::move(success_callback_wrapper),
                                            std::move(error_callback_wrapper),
                                            &worker);

  LocalFileSystem::From(worker)->ResolveURL(completed_url, std::move(callbacks),
                                            LocalFileSystem::kSynchronous);

  Entry* entry = sync_helper->GetResultOrThrow(exception_state);
  return entry ? EntrySync::Create(entry) : nullptr;
}

static_assert(static_cast<int>(WorkerGlobalScopeFileSystem::kTemporary) ==
                  static_cast<int>(mojom::blink::FileSystemType::kTemporary),
              "WorkerGlobalScopeFileSystem::kTemporary should match "
              "FileSystemTypeTemporary");
static_assert(static_cast<int>(WorkerGlobalScopeFileSystem::kPersistent) ==
                  static_cast<int>(mojom::blink::FileSystemType::kPersistent),
              "WorkerGlobalScopeFileSystem::kPersistent should match "
              "FileSystemTypePersistent");

}  // namespace blink
