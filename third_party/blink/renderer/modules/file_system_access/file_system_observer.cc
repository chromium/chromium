// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_observer.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_observer_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_observer_observe_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_manager.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_change_record.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
FileSystemObserver* FileSystemObserver::Create(
    ScriptState* script_state,
    V8FileSystemObserverCallback* callback,
    ExceptionState& exception_state) {
  auto* context = ExecutionContext::From(script_state);

  SECURITY_CHECK(context->IsWindow() ||
                 context->IsDedicatedWorkerGlobalScope() ||
                 context->IsSharedWorkerGlobalScope());

  if (!context->GetSecurityOrigin()->CanAccessFileSystem()) {
    if (context->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
      exception_state.ThrowSecurityError(
          "File system access is denied because the context is "
          "sandboxed and lacks the 'allow-same-origin' flag.");
      return nullptr;
    } else {
      exception_state.ThrowSecurityError("File system access is denied.");
      return nullptr;
    }
  }

  mojo::PendingRemote<mojom::blink::FileSystemAccessObserverHost> host_remote;
  mojo::PendingReceiver<mojom::blink::FileSystemAccessObserverHost>
      host_receiver = host_remote.InitWithNewPipeAndPassReceiver();

  auto* observer_host = MakeGarbageCollected<FileSystemObserver>(
      context, callback, std::move(host_remote));

  FileSystemAccessManager::From(context)->BindObserverHost(
      std::move(host_receiver));
  return observer_host;
}

FileSystemObserver::FileSystemObserver(
    ExecutionContext* context,
    V8FileSystemObserverCallback* callback,
    mojo::PendingRemote<mojom::blink::FileSystemAccessObserverHost> host_remote)
    : execution_context_(context),
      callback_(callback),
      observer_receivers_(this, context),
      host_remote_(context) {
  host_remote_.Bind(std::move(host_remote),
                    execution_context_->GetTaskRunner(TaskType::kStorage));
}

ScriptPromise FileSystemObserver::observe(
    ScriptState* script_state,
    FileSystemHandle* handle,
    FileSystemObserverObserveOptions* options,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1019297): Add AllowStorageAccess checks.

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  host_remote_->Observe(
      handle->Transfer(), options->recursive(),
      WTF::BindOnce(&FileSystemObserver::DidObserve, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return result;
}

void FileSystemObserver::DidObserve(
    ScriptPromiseResolver* resolver,
    mojom::blink::FileSystemAccessErrorPtr result,
    mojo::PendingReceiver<mojom::blink::FileSystemAccessObserver>
        observer_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    file_system_access_error::Reject(resolver, *result);
    return;
  }

  observer_receivers_.Add(
      std::move(observer_receiver),
      execution_context_->GetTaskRunner(TaskType::kStorage));

  resolver->Resolve();
}

void FileSystemObserver::unobserve(FileSystemHandle* handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!host_remote_.is_bound()) {
    return;
  }

  // TODO(https://crbug.com/1019297): Unqueue and pause records for this
  // observation.

  // Disconnects the receiver of an observer corresponding to `handle`, if such
  // an observer exists. This will remove it from our `observer_receivers_` set.
  // It would be nice to disconnect the receiver from here, but we don't know
  // which observer (if any) corresponds to `handle` without hopping to the
  // browser to validate `handle`.
  host_remote_->Unobserve(handle->Transfer());
}

void FileSystemObserver::disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_receivers_.Clear();
}

void FileSystemObserver::OnFileChanges(
    WTF::Vector<mojom::blink::FileSystemAccessChangePtr> mojo_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HeapVector<Member<FileSystemChangeRecord>> records;
  for (auto& mojo_change : mojo_changes) {
    FileSystemHandle* root_handle = FileSystemHandle::CreateFromMojoEntry(
        std::move(mojo_change->metadata->root), execution_context_);
    FileSystemHandle* changed_file_handle =
        FileSystemHandle::CreateFromMojoEntry(
            std::move(mojo_change->metadata->changed_entry),
            execution_context_);

    auto* record = MakeGarbageCollected<FileSystemChangeRecord>(
        root_handle, changed_file_handle,
        std::move(mojo_change->metadata->relative_path),
        std::move(mojo_change->type));
    records.push_back(record);
  }

  callback_->InvokeAndReportException(this, std::move(records), this);
}

void FileSystemObserver::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(callback_);
  visitor->Trace(observer_receivers_);
  visitor->Trace(host_remote_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
