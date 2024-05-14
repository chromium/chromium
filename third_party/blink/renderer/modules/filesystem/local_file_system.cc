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
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

void LocalFileSystem::ResolveURL(const KURL& file_system_url,
                                 std::unique_ptr<ResolveURICallbacks> callbacks,
                                 SynchronousType type) {
  RequestFileSystemAccessInternal(
      WTF::BindOnce(&LocalFileSystem::ResolveURLCallback,
                    MakeUnwrappingCrossThreadHandle(this), file_system_url,
                    std::move(callbacks), type));
}

void LocalFileSystem::ResolveURLCallback(
    const KURL& file_system_url,
    std::unique_ptr<ResolveURICallbacks> callbacks,
    SynchronousType sync_type,
    bool allowed) {
  if (allowed) {
    ResolveURLInternal(file_system_url, std::move(callbacks), sync_type);
    return;
  }
  FileSystemNotAllowedInternal(std::move(callbacks));
}

void LocalFileSystem::RequestFileSystem(
    mojom::blink::FileSystemType type,
    int64_t size,
    std::unique_ptr<FileSystemCallbacks> callbacks,
    SynchronousType sync_type) {
  RequestFileSystemAccessInternal(
      WTF::BindOnce(&LocalFileSystem::RequestFileSystemCallback,
                    MakeUnwrappingCrossThreadHandle(this), type,
                    std::move(callbacks), sync_type));
}

void LocalFileSystem::RequestFileSystemCallback(
    mojom::blink::FileSystemType type,
    std::unique_ptr<FileSystemCallbacks> callbacks,
    SynchronousType sync_type,
    bool allowed) {
  if (allowed) {
    FileSystemAllowedInternal(type, std::move(callbacks), sync_type);
    return;
  }
  FileSystemNotAllowedInternal(std::move(callbacks));
}

void LocalFileSystem::RequestFileSystemAccessInternal(
    base::OnceCallback<void(bool)> callback) {
  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(GetSupplementable())) {
    window->GetFrame()->AllowStorageAccessAndNotify(
        WebContentSettingsClient::StorageType::kFileSystem,
        std::move(callback));
    return;
  }
  if (auto* global_scope = DynamicTo<WorkerGlobalScope>(GetSupplementable())) {
    auto* client = global_scope->ContentSettingsClient();
    if (!client) {
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(client->AllowStorageAccessSync(
          WebContentSettingsClient::StorageType::kFileSystem));
    }
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void LocalFileSystem::FileSystemNotAllowedInternal(
    std::unique_ptr<FileSystemCallbacks> callbacks) {
  GetSupplementable()
      ->GetTaskRunner(TaskType::kFileReading)
      ->PostTask(FROM_HERE, WTF::BindOnce(&FileSystemCallbacks::DidFail,
                                          std::move(callbacks),
                                          base::File::FILE_ERROR_ABORT));
}

void LocalFileSystem::FileSystemNotAllowedInternal(
    std::unique_ptr<ResolveURICallbacks> callbacks) {
  GetSupplementable()
      ->GetTaskRunner(TaskType::kFileReading)
      ->PostTask(FROM_HERE, WTF::BindOnce(&ResolveURICallbacks::DidFail,
                                          std::move(callbacks),
                                          base::File::FILE_ERROR_ABORT));
}

void LocalFileSystem::FileSystemAllowedInternal(
    mojom::blink::FileSystemType type,
    std::unique_ptr<FileSystemCallbacks> callbacks,
    SynchronousType sync_type) {
  ExecutionContext* context = GetSupplementable();
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
    const KURL& file_system_url,
    std::unique_ptr<ResolveURICallbacks> callbacks,
    SynchronousType sync_type) {
  FileSystemDispatcher& dispatcher =
      FileSystemDispatcher::From(GetSupplementable());
  if (sync_type == kSynchronous) {
    dispatcher.ResolveURLSync(file_system_url, std::move(callbacks));
  } else {
    dispatcher.ResolveURL(file_system_url, std::move(callbacks));
  }
}

LocalFileSystem::LocalFileSystem(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

const char LocalFileSystem::kSupplementName[] = "LocalFileSystem";

LocalFileSystem* LocalFileSystem::From(ExecutionContext& context) {
  LocalFileSystem* file_system =
      Supplement<ExecutionContext>::From<LocalFileSystem>(context);
  if (!file_system) {
    file_system = MakeGarbageCollected<LocalFileSystem>(context);
    Supplement<ExecutionContext>::ProvideTo(context, file_system);
  }
  return file_system;
}

}  // namespace blink
