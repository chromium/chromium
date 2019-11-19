/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/local_file_system.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

LocalFileSystem::~LocalFileSystem() = default;

void LocalFileSystem::ResolveURL(ExecutionContext* context,
                                 const KURL& file_system_url,
                                 std::unique_ptr<ResolveURICallbacks> callbacks,
                                 SynchronousType type) {
  RequestFileSystemAccessInternal(
      context,
      WTF::Bind(&LocalFileSystem::ResolveURLCallback,
                WrapCrossThreadPersistent(this), WrapPersistent(context),
                file_system_url, std::move(callbacks), type));
}

void LocalFileSystem::ResolveURLCallback(
    ExecutionContext* context,
    const KURL& file_system_url,
    std::unique_ptr<ResolveURICallbacks> callbacks,
    SynchronousType sync_type,
    bool allowed) {
  if (allowed) {
    ResolveURLInternal(context, file_system_url, std::move(callbacks),
                       sync_type);
    return;
  }
  FileSystemNotAllowedInternal(context, std::move(callbacks));
}

void LocalFileSystem::RequestFileSystem(
    ExecutionContext* context,
    mojom::blink::FileSystemType type,
    int64_t size,
    std::unique_ptr<FileSystemCallbacks> callbacks,
    SynchronousType sync_type) {
  RequestFileSystemAccessInternal(
      context,
      WTF::Bind(&LocalFileSystem::RequestFileSystemCallback,
                WrapCrossThreadPersistent(this), WrapPersistent(context), type,
                std::move(callbacks), sync_type));
}

void LocalFileSystem::RequestFileSystemCallback(
    ExecutionContext* context,
    mojom::blink::FileSystemType type,
    std::unique_ptr<FileSystemCallbacks> callbacks,
    SynchronousType sync_type,
    bool allowed) {
  if (allowed) {
    FileSystemAllowedInternal(context, type, std::move(callbacks), sync_type);
    return;
  }
  FileSystemNotAllowedInternal(context, std::move(callbacks));
}

void LocalFileSystem::RequestFileSystemAccessInternal(
    ExecutionContext* context,
    base::OnceCallback<void(bool)> callback) {
  if (context->IsDocument()) {
    auto* client =
        To<Document>(context)->GetFrame()->GetContentSettingsClient();
    if (!client) {
      std::move(callback).Run(true);
    } else {
      client->RequestFileSystemAccessAsync(std::move(callback));
    }
    return;
  }
  if (context->IsWorkerGlobalScope()) {
    auto* client = To<WorkerGlobalScope>(context)->ContentSettingsClient();
    if (!client) {
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(client->RequestFileSystemAccessSync());
    }
    return;
  }
  NOTREACHED();
}

void LocalFileSystem::FileSystemNotAllowedInternal(
    ExecutionContext* context,
    std::unique_ptr<FileSystemCallbacks> callbacks) {
  context->GetTaskRunner(TaskType::kFileReading)
      ->PostTask(FROM_HERE, WTF::Bind(&FileSystemCallbacks::DidFail,
                                      WTF::Passed(std::move(callbacks)),
                                      base::File::FILE_ERROR_ABORT));
}

void LocalFileSystem::FileSystemNotAllowedInternal(
    ExecutionContext* context,
    std::unique_ptr<ResolveURICallbacks> callbacks) {
  context->GetTaskRunner(TaskType::kFileReading)
      ->PostTask(FROM_HERE, WTF::Bind(&ResolveURICallbacks::DidFail,
                                      WTF::Passed(std::move(callbacks)),
                                      base::File::FILE_ERROR_ABORT));
}

void LocalFileSystem::FileSystemAllowedInternal(
    ExecutionContext* context,
    mojom::blink::FileSystemType type,
    std::unique_ptr<FileSystemCallbacks> callbacks,
    SynchronousType sync_type) {
  FileSystemDispatcher& dispatcher = FileSystemDispatcher::From(context);
  if (sync_type == kSynchronous) {
    dispatcher.OpenFileSystemSync(context->GetSecurityOrigin(), type,
                                  std::move(callbacks));
  } else {
    dispatcher.OpenFileSystem(context->GetSecurityOrigin(), type,
                              std::move(callbacks));
  }
}

void LocalFileSystem::ResolveURLInternal(
    ExecutionContext* context,
    const KURL& file_system_url,
    std::unique_ptr<ResolveURICallbacks> callbacks,
    SynchronousType sync_type) {
  FileSystemDispatcher& dispatcher = FileSystemDispatcher::From(context);
  if (sync_type == kSynchronous) {
    dispatcher.ResolveURLSync(file_system_url, std::move(callbacks));
  } else {
    dispatcher.ResolveURL(file_system_url, std::move(callbacks));
  }
}

LocalFileSystem::LocalFileSystem(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

LocalFileSystem::LocalFileSystem(WorkerGlobalScope& worker_global_scope)
    : Supplement<WorkerGlobalScope>(worker_global_scope) {}

void LocalFileSystem::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  Supplement<WorkerGlobalScope>::Trace(visitor);
}

const char LocalFileSystem::kSupplementName[] = "LocalFileSystem";

LocalFileSystem* LocalFileSystem::From(ExecutionContext& context) {
  if (auto* document = DynamicTo<Document>(context)) {
    LocalFileSystem* file_system =
        Supplement<LocalFrame>::From<LocalFileSystem>(document->GetFrame());
    DCHECK(file_system);
    return file_system;
  }

  LocalFileSystem* file_system =
      Supplement<WorkerGlobalScope>::From<LocalFileSystem>(
          To<WorkerGlobalScope>(context));
  DCHECK(file_system);
  return file_system;
}

void ProvideLocalFileSystemTo(LocalFrame& frame) {
  frame.ProvideSupplement(MakeGarbageCollected<LocalFileSystem>(frame));
}

void ProvideLocalFileSystemToWorker(WorkerGlobalScope& worker_global_scope) {
  Supplement<WorkerGlobalScope>::ProvideTo(
      worker_global_scope,
      MakeGarbageCollected<LocalFileSystem>(worker_global_scope));
}

}  // namespace blink
