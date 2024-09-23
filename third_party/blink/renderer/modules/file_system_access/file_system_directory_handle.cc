// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_get_directory_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_get_file_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_remove_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FileSystemDirectoryHandle::IterationSource final
    : public PairAsyncIterable<FileSystemDirectoryHandle>::IterationSource,
      public ExecutionContextClient,
      public mojom::blink::FileSystemAccessDirectoryEntriesListener {
 public:
  IterationSource(ScriptState* script_state,
                  ExecutionContext* execution_context,
                  Kind kind,
                  FileSystemDirectoryHandle* directory)
      : PairAsyncIterable<FileSystemDirectoryHandle>::IterationSource(
            script_state,
            kind),
        ExecutionContextClient(execution_context),
        directory_(directory),
        receiver_(this, execution_context),
        keep_alive_(this) {
    directory_->MojoHandle()->GetEntries(receiver_.BindNewPipeAndPassRemote(
        execution_context->GetTaskRunner(TaskType::kStorage)));
  }

  void DidReadDirectory(mojom::blink::FileSystemAccessErrorPtr result,
                        Vector<mojom::blink::FileSystemAccessEntryPtr> entries,
                        bool has_more_entries) override {
    is_waiting_for_more_entries_ = has_more_entries;
    ExecutionContext* const execution_context = GetExecutionContext();
    if (!has_more_entries || !execution_context) {
      keep_alive_.Clear();
    }
    if (!execution_context) {
      return;
    }
    if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
      for (auto& entry : entries) {
        file_system_handle_queue_.push_back(
            FileSystemHandle::CreateFromMojoEntry(std::move(entry),
                                                  execution_context));
      }
    } else {
      CHECK(!has_more_entries);
      error_ = std::move(result);
    }
    ScriptState::Scope script_state_scope(GetScriptState());
    TryResolvePromise();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(directory_);
    visitor->Trace(receiver_);
    visitor->Trace(file_system_handle_queue_);
    PairAsyncIterable<FileSystemDirectoryHandle>::IterationSource::Trace(
        visitor);
    ExecutionContextClient::Trace(visitor);
  }

 protected:
  void GetNextIterationResult() override { TryResolvePromise(); }

 private:
  void TryResolvePromise() {
    if (!HasPendingPromise()) {
      return;
    }

    if (!file_system_handle_queue_.empty()) {
      FileSystemHandle* handle = file_system_handle_queue_.TakeFirst();
      TakePendingPromiseResolver()->Resolve(
          MakeIterationResult(handle->name(), handle));
      return;
    }

    if (error_) {
      file_system_access_error::Reject(TakePendingPromiseResolver(), *error_);
      return;
    }

    if (!is_waiting_for_more_entries_) {
      TakePendingPromiseResolver()->Resolve(MakeEndOfIteration());
      return;
    }
  }

  Member<FileSystemDirectoryHandle> directory_;
  HeapMojoReceiver<mojom::blink::FileSystemAccessDirectoryEntriesListener,
                   IterationSource>
      receiver_;
  // Queue of the successful results.
  HeapDeque<Member<FileSystemHandle>> file_system_handle_queue_;
  mojom::blink::FileSystemAccessErrorPtr error_;
  bool is_waiting_for_more_entries_ = true;
  // HeapMojoReceived won't retain us, so maintain a keepalive while
  // waiting for the browser to send all entries.
  SelfKeepAlive<IterationSource> keep_alive_;
};

using mojom::blink::FileSystemAccessErrorPtr;

FileSystemDirectoryHandle::FileSystemDirectoryHandle(
    ExecutionContext* context,
    const String& name,
    mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle> mojo_ptr)
    : FileSystemHandle(context, name), mojo_ptr_(context) {
  mojo_ptr_.Bind(std::move(mojo_ptr),
                 context->GetTaskRunner(TaskType::kStorage));
  DCHECK(mojo_ptr_.is_bound());
}

ScriptPromise<FileSystemFileHandle> FileSystemDirectoryHandle::getFileHandle(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetFileOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemFileHandle>>(
          script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  mojo_ptr_->GetFile(
      name, options->create(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*,
             ScriptPromiseResolver<FileSystemFileHandle>* resolver,
             const String& name, FileSystemAccessErrorPtr result,
             mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle>
                 handle) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MakeGarbageCollected<FileSystemFileHandle>(
                context, name, std::move(handle)));
          },
          WrapPersistent(this), WrapPersistent(resolver), name));

  return result;
}

ScriptPromise<FileSystemDirectoryHandle>
FileSystemDirectoryHandle::getDirectoryHandle(
    ScriptState* script_state,
    const String& name,
    const FileSystemGetDirectoryOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemDirectoryHandle>>(
          script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  mojo_ptr_->GetDirectory(
      name, options->create(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*,
             ScriptPromiseResolver<FileSystemDirectoryHandle>* resolver,
             const String& name, FileSystemAccessErrorPtr result,
             mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle>
                 handle) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context) {
              return;
            }
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MakeGarbageCollected<FileSystemDirectoryHandle>(
                context, name, std::move(handle)));
          },
          WrapPersistent(this), WrapPersistent(resolver), name));

  return result;
}

ScriptPromise<IDLUndefined> FileSystemDirectoryHandle::removeEntry(
    ScriptState* script_state,
    const String& name,
    const FileSystemRemoveOptions* options,
    ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  mojo_ptr_->RemoveEntry(name, options->recursive(),
                         WTF::BindOnce(
                             [](FileSystemDirectoryHandle*,
                                ScriptPromiseResolver<IDLUndefined>* resolver,
                                FileSystemAccessErrorPtr result) {
                               // Keep `this` alive so the handle will not be
                               // garbage-collected before the promise is
                               // resolved.
                               file_system_access_error::ResolveOrReject(
                                   resolver, *result);
                             },
                             WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise<IDLNullable<IDLSequence<IDLUSVString>>>
FileSystemDirectoryHandle::resolve(ScriptState* script_state,
                                   FileSystemHandle* possible_child,
                                   ExceptionState& exception_state) {
  if (!mojo_ptr_.is_bound()) {
    // TODO(crbug.com/1293949): Add an error message.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, "");
    return ScriptPromise<IDLNullable<IDLSequence<IDLUSVString>>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<IDLSequence<IDLUSVString>>>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  mojo_ptr_->Resolve(
      possible_child->Transfer(),
      WTF::BindOnce(
          [](FileSystemDirectoryHandle*,
             ScriptPromiseResolver<IDLNullable<IDLSequence<IDLUSVString>>>*
                 resolver,
             FileSystemAccessErrorPtr result,
             const std::optional<Vector<String>>& path) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            if (!path.has_value()) {
              resolver->Resolve(std::nullopt);
              return;
            }
            resolver->Resolve(*path);
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>
FileSystemDirectoryHandle::Transfer() {
  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> result;
  if (mojo_ptr_.is_bound()) {
    mojo_ptr_->Transfer(result.InitWithNewPipeAndPassReceiver());
  }
  return result;
}

void FileSystemDirectoryHandle::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  FileSystemHandle::Trace(visitor);
}

void FileSystemDirectoryHandle::QueryPermissionImpl(
    bool writable,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
    return;
  }
  mojo_ptr_->GetPermissionStatus(writable, std::move(callback));
}

void FileSystemDirectoryHandle::RequestPermissionImpl(
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

void FileSystemDirectoryHandle::MoveImpl(
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

void FileSystemDirectoryHandle::RemoveImpl(
    const FileSystemRemoveOptions* options,
    base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)> callback) {
  if (!mojo_ptr_.is_bound()) {
    std::move(callback).Run(mojom::blink::FileSystemAccessError::New(
        mojom::blink::FileSystemAccessStatus::kInvalidState,
        base::File::Error::FILE_ERROR_FAILED, "Context Destroyed"));
    return;
  }

  mojo_ptr_->Remove(options->recursive(), std::move(callback));
}

void FileSystemDirectoryHandle::IsSameEntryImpl(
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

  mojo_ptr_->Resolve(
      std::move(other),
      WTF::BindOnce(
          [](base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                                     bool)> callback,
             FileSystemAccessErrorPtr result,
             const std::optional<Vector<String>>& path) {
            std::move(callback).Run(std::move(result),
                                    path.has_value() && path->empty());
          },
          std::move(callback)));
}

void FileSystemDirectoryHandle::GetUniqueIdImpl(
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

void FileSystemDirectoryHandle::GetCloudIdentifiersImpl(
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

PairAsyncIterable<FileSystemDirectoryHandle>::IterationSource*
FileSystemDirectoryHandle::CreateIterationSource(
    ScriptState* script_state,
    PairAsyncIterable<FileSystemDirectoryHandle>::IterationSource::Kind kind,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The window is detached.");
    return nullptr;
  }

  return MakeGarbageCollected<IterationSource>(script_state, execution_context,
                                               kind, this);
}

}  // namespace blink
