// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file_manager.h"

#include <utility>

#include "base/files/file.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "third_party/blink/renderer/modules/native_io/native_io_error.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file_sync.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool IsValidNativeIONameCharacter(int name_char) {
  return (name_char >= 'a' && name_char <= 'z') ||
         (name_char >= '0' && name_char <= '9') || name_char == '_';
}

// Maximum allowed filename length, inclusive.
const int kMaximumFilenameLength = 100;

bool IsValidNativeIOName(const String& name) {
  if (name.empty())
    return false;

  if (name.length() > kMaximumFilenameLength)
    return false;

  if (name.Is8Bit()) {
    return base::ranges::all_of(name.Span8(), &IsValidNativeIONameCharacter);
  }
  return base::ranges::all_of(name.Span16(), &IsValidNativeIONameCharacter);
}

void ThrowStorageAccessError(ExceptionState& exception_state) {
  // TODO(fivedots): Switch to security error after it's available as a
  // NativeIOErrorType.
  ThrowNativeIOWithError(exception_state,
                         mojom::blink::NativeIOError::New(
                             mojom::blink::NativeIOErrorType::kUnknown,
                             "Storage access is denied"));
}

void OnGetAllResult(ScriptPromiseResolver* resolver,
                    bool backend_success,
                    const Vector<String>& file_names) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!backend_success) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        "getAll() failed"));
    return;
  }

  resolver->Resolve(file_names);
}

void OnRenameResult(ScriptPromiseResolver* resolver,
                    mojom::blink::NativeIOErrorPtr rename_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (rename_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    blink::RejectNativeIOWithError(resolver, std::move(rename_error));
    return;
  }
  resolver->Resolve();
}

}  // namespace

NativeIOFileManager::NativeIOFileManager(
    ExecutionContext* execution_context,
    HeapMojoRemote<mojom::blink::NativeIOHost> backend,
    NativeIOCapacityTracker* capacity_tracker)
    : ExecutionContextClient(execution_context),
      capacity_tracker_(capacity_tracker),
      // TODO(pwnall): Get a dedicated queue when the specification matures.
      receiver_task_runner_(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      backend_(std::move(backend)) {
  backend_.set_disconnect_handler(WTF::BindOnce(
      &NativeIOFileManager::OnBackendDisconnect, WrapWeakPersistent(this)));
}

NativeIOFileManager::~NativeIOFileManager() = default;

ScriptPromise NativeIOFileManager::open(ScriptState* script_state,
                                        String name,
                                        ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return ScriptPromise();
  }

  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return ScriptPromise();
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::OpenImpl, WrapWeakPersistent(this),
                    name, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise NativeIOFileManager::Delete(ScriptState* script_state,
                                          String name,
                                          ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return ScriptPromise();
  }

  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::DeleteImpl, WrapWeakPersistent(this),
                    name, WrapPersistent(resolver)));

  return promise;
}

ScriptPromise NativeIOFileManager::getAll(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::GetAllImpl, WrapWeakPersistent(this),
                    WrapPersistent(resolver)));

  return promise;
}

ScriptPromise NativeIOFileManager::rename(ScriptState* script_state,
                                          String old_name,
                                          String new_name,
                                          ExceptionState& exception_state) {
  if (!IsValidNativeIOName(old_name) || !IsValidNativeIOName(new_name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return ScriptPromise();
  }

  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::RenameImpl, WrapWeakPersistent(this),
                    old_name, new_name, WrapPersistent(resolver)));

  return promise;
}

NativeIOFileSync* NativeIOFileManager::openSync(
    String name,
    ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return nullptr;
  }

  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return nullptr;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return nullptr;
  }

  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file(
      execution_context);
  mojo::PendingReceiver<mojom::blink::NativeIOFileHost> backend_file_receiver =
      backend_file.BindNewPipeAndPassReceiver(receiver_task_runner_);

  base::File backing_file;
  uint64_t backing_file_length;
  mojom::blink::NativeIOErrorPtr open_error;
  bool call_succeeded =
      backend_->OpenFile(name, std::move(backend_file_receiver), &backing_file,
                         &backing_file_length, &open_error);

  if (open_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    ThrowNativeIOWithError(exception_state, std::move(open_error));
    return nullptr;
  }
  DCHECK(call_succeeded) << "Mojo call failed";
  DCHECK(backing_file.IsValid()) << "File is invalid but no error set";

  return MakeGarbageCollected<NativeIOFileSync>(
      std::move(backing_file), base::as_signed(backing_file_length),
      std::move(backend_file), capacity_tracker_.Get(), execution_context);
}

void NativeIOFileManager::deleteSync(String name,
                                     ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return;
  }

  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return;
  }

  mojom::blink::NativeIOErrorPtr delete_error;
  uint64_t deleted_file_size;
  bool call_succeeded =
      backend_->DeleteFile(name, &delete_error, &deleted_file_size);

  if (delete_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    ThrowNativeIOWithError(exception_state, std::move(delete_error));
    return;
  }
  DCHECK(call_succeeded) << "Mojo call failed";

  if (deleted_file_size > 0) {
    capacity_tracker_->ChangeAvailableCapacity(
        base::as_signed(deleted_file_size));
  }
}

Vector<String> NativeIOFileManager::getAllSync(
    ExceptionState& exception_state) {
  Vector<String> result;
  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return result;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return result;
  }

  bool backend_success = false;
  bool call_succeeded = backend_->GetAllFileNames(&backend_success, &result);
  DCHECK(call_succeeded) << "Mojo call failed";

  if (!backend_success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "getAllSync() failed");
  }
  return result;
}

void NativeIOFileManager::renameSync(String old_name,
                                     String new_name,
                                     ExceptionState& exception_state) {
  if (!IsValidNativeIOName(old_name) || !IsValidNativeIOName(new_name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return;
  }

  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return;
  }

  mojom::blink::NativeIOErrorPtr backend_success;
  bool call_succeeded =
      backend_->RenameFile(old_name, new_name, &backend_success);

  if (backend_success->type != mojom::blink::NativeIOErrorType::kSuccess) {
    ThrowNativeIOWithError(exception_state, std::move(backend_success));
    return;
  }
  DCHECK(call_succeeded) << "Mojo call failed";
}

ScriptPromise NativeIOFileManager::requestCapacity(
    ScriptState* script_state,
    uint64_t requested_capacity,
    ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return ScriptPromise();
  }
  if (!base::IsValueInRangeForNumericType<int64_t>(requested_capacity)) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kNoSpace,
                               "No capacity available for this operation"));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::RequestCapacityImpl,
                    WrapWeakPersistent(this), requested_capacity,
                    WrapPersistent(resolver)));

  return promise;
}

ScriptPromise NativeIOFileManager::releaseCapacity(
    ScriptState* script_state,
    uint64_t requested_release,
    ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::ReleaseCapacityImpl,
                    WrapWeakPersistent(this), requested_release,
                    WrapPersistent(resolver)));

  return promise;
}

ScriptPromise NativeIOFileManager::getRemainingCapacity(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "Execution context went away"));
    return ScriptPromise();
  }

  CheckStorageAccessAllowed(
      execution_context, resolver,
      WTF::BindOnce(&NativeIOFileManager::GetRemainingCapacityImpl,
                    WrapWeakPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void NativeIOFileManager::CheckStorageAccessAllowed(
    ExecutionContext* context,
    ScriptPromiseResolver* resolver,
    base::OnceCallback<void()> callback) {
  DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  auto wrapped_callback = WTF::BindOnce(
      &NativeIOFileManager::DidCheckStorageAccessAllowed,
      WrapWeakPersistent(this), WrapPersistent(resolver), std::move(callback));

  // TODO(crbug/1180185): consider removing caching, if the worker
  // WebContentSettingsClient does it for us.
  if (storage_access_allowed_.has_value()) {
    std::move(wrapped_callback).Run(storage_access_allowed_.value());
    return;
  }

  WebContentSettingsClient* content_settings_client = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      std::move(wrapped_callback).Run(false);
      return;
    }
    content_settings_client = frame->GetContentSettingsClient();
  } else {
    content_settings_client =
        To<WorkerGlobalScope>(context)->ContentSettingsClient();
  }

  // TODO(fivedots): Switch storage type once we stop aliasing under Filesystem.
  if (content_settings_client) {
    content_settings_client->AllowStorageAccess(
        WebContentSettingsClient::StorageType::kFileSystem,
        std::move(wrapped_callback));
    return;
  }
  std::move(wrapped_callback).Run(true);
}

void NativeIOFileManager::DidCheckStorageAccessAllowed(
    ScriptPromiseResolver* resolver,
    base::OnceCallback<void()> callback,
    bool allowed_access) {
  storage_access_allowed_ = allowed_access;

  if (allowed_access) {
    std::move(callback).Run();
    return;
  }

  blink::RejectNativeIOWithError(resolver,
                                 mojom::blink::NativeIOError::New(
                                     mojom::blink::NativeIOErrorType::kUnknown,
                                     "Storage access is denied"));
  return;
}

bool NativeIOFileManager::CheckStorageAccessAllowedSync(
    ExecutionContext* context) {
  DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  if (storage_access_allowed_.has_value()) {
    return storage_access_allowed_.value();
  }

  WebContentSettingsClient* content_settings_client = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      return false;
    }
    content_settings_client = frame->GetContentSettingsClient();
  } else {
    content_settings_client =
        To<WorkerGlobalScope>(context)->ContentSettingsClient();
  }

  if (content_settings_client) {
    return content_settings_client->AllowStorageAccessSync(
        WebContentSettingsClient::StorageType::kFileSystem);
  }
  return true;
}

uint64_t NativeIOFileManager::requestCapacitySync(
    uint64_t requested_capacity,
    ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return 0;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return 0;
  }

  if (!base::IsValueInRangeForNumericType<int64_t>(requested_capacity)) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kNoSpace,
                               "No capacity available for this operation"));
    return 0;
  }

  int64_t granted_capacity_delta;

  bool call_succeeded = backend_->RequestCapacityChange(
      requested_capacity, &granted_capacity_delta);
  DCHECK(call_succeeded) << "Mojo call failed";

  capacity_tracker_->ChangeAvailableCapacity(granted_capacity_delta);
  return capacity_tracker_->GetAvailableCapacity();
}

uint64_t NativeIOFileManager::releaseCapacitySync(
    uint64_t requested_release,
    ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "NativeIOHost backend went away"));
    return 0;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return 0;
  }

  if (!base::IsValueInRangeForNumericType<int64_t>(requested_release)) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kNoSpace,
                               "No capacity available for this operation"));
    return 0;
  }

  int64_t requested_difference = -base::as_signed(requested_release);

  // Reducing available capacity is done before performing the IPC for symmetry
  // with the async operation.
  if (!capacity_tracker_->ChangeAvailableCapacity(requested_difference)) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kNoSpace,
                               "No capacity available for this operation"));
    return 0;
  }

  // As `requested_difference` < 0, `granted_capacity_delta` is guaranteed to
  // equal `requested_difference`.
  int64_t granted_capacity_delta;
  bool call_succeeded = backend_->RequestCapacityChange(
      requested_difference, &granted_capacity_delta);
  DCHECK(call_succeeded) << "Mojo call failed";

  return capacity_tracker_->GetAvailableCapacity();
}

uint64_t NativeIOFileManager::getRemainingCapacitySync(
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "Execution context went away"));
    return 0;
  }

  if (!CheckStorageAccessAllowedSync(execution_context)) {
    ThrowStorageAccessError(exception_state);
    return 0;
  }

  uint64_t available_capacity =
      base::as_unsigned(capacity_tracker_->GetAvailableCapacity());
  return available_capacity;
}

void NativeIOFileManager::OnBackendDisconnect() {
  backend_.reset();
}

void NativeIOFileManager::OnOpenResult(
    ScriptPromiseResolver* resolver,
    DisallowNewWrapper<HeapMojoRemote<mojom::blink::NativeIOFileHost>>*
        backend_file_wrapper,
    base::File backing_file,
    uint64_t backing_file_length,
    mojom::blink::NativeIOErrorPtr open_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (open_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    blink::RejectNativeIOWithError(resolver, std::move(open_error));
    return;
  }
  DCHECK(backing_file.IsValid()) << "browser returned closed file but no error";

  NativeIOFile* file = MakeGarbageCollected<NativeIOFile>(
      std::move(backing_file), base::as_signed(backing_file_length),
      backend_file_wrapper->TakeValue(), capacity_tracker_.Get(),
      ExecutionContext::From(script_state));
  resolver->Resolve(file);
}

void NativeIOFileManager::OnDeleteResult(
    ScriptPromiseResolver* resolver,
    mojom::blink::NativeIOErrorPtr delete_error,
    uint64_t deleted_file_size) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (delete_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    blink::RejectNativeIOWithError(resolver, std::move(delete_error));
    return;
  }

  if (deleted_file_size > 0) {
    capacity_tracker_->ChangeAvailableCapacity(
        base::as_signed(deleted_file_size));
  }

  resolver->Resolve();
}

void NativeIOFileManager::OnRequestCapacityChangeResult(
    ScriptPromiseResolver* resolver,
    int64_t granted_capacity) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  // If `granted_capacity` < 0, the available capacity has already been released
  // prior to the IPC.
  if (granted_capacity > 0) {
    capacity_tracker_->ChangeAvailableCapacity(
        base::as_signed(granted_capacity));
  }
  uint64_t available_capacity = capacity_tracker_->GetAvailableCapacity();

  resolver->Resolve(available_capacity);
}

void NativeIOFileManager::OpenImpl(String name,
                                   ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_.is_bound()) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kInvalidState,
                      "NativeIOHost backend went away"));
    return;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file(
      execution_context);
  mojo::PendingReceiver<mojom::blink::NativeIOFileHost> backend_file_receiver =
      backend_file.BindNewPipeAndPassReceiver(receiver_task_runner_);

  backend_->OpenFile(
      name, std::move(backend_file_receiver),
      WTF::BindOnce(&NativeIOFileManager::OnOpenResult, WrapPersistent(this),
                    WrapPersistent(resolver),
                    WrapPersistent(WrapDisallowNew(std::move(backend_file)))));
}

void NativeIOFileManager::DeleteImpl(String name,
                                     ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_.is_bound()) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kInvalidState,
                      "NativeIOHost backend went away"));
    return;
  }

  backend_->DeleteFile(
      name, WTF::BindOnce(&NativeIOFileManager::OnDeleteResult,
                          WrapPersistent(this), WrapPersistent(resolver)));
}

void NativeIOFileManager::GetAllImpl(ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_.is_bound()) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kInvalidState,
                      "NativeIOHost backend went away"));
    return;
  }

  backend_->GetAllFileNames(
      WTF::BindOnce(&OnGetAllResult, WrapPersistent(resolver)));
}

void NativeIOFileManager::RenameImpl(String old_name,
                                     String new_name,
                                     ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_.is_bound()) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kInvalidState,
                      "NativeIOHost backend went away"));
    return;
  }

  backend_->RenameFile(
      old_name, new_name,
      WTF::BindOnce(&OnRenameResult, WrapPersistent(resolver)));
}

void NativeIOFileManager::RequestCapacityImpl(uint64_t requested_capacity,
                                              ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_.is_bound()) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kInvalidState,
                      "NativeIOHost backend went away"));
    return;
  }

  backend_->RequestCapacityChange(
      requested_capacity,
      WTF::BindOnce(&NativeIOFileManager::OnRequestCapacityChangeResult,
                    WrapPersistent(this), WrapPersistent(resolver)));
}

void NativeIOFileManager::ReleaseCapacityImpl(uint64_t requested_release,
                                              ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_.is_bound()) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kInvalidState,
                      "NativeIOHost backend went away"));
    return;
  }

  if (!base::IsValueInRangeForNumericType<int64_t>(requested_release)) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kNoSpace,
                      "Attempted to release more capacity than available"));
    return;
  }

  int64_t requested_difference = -base::as_signed(requested_release);

  // Reducing available capacity must be done before performing the IPC, so
  // capacity cannot be double-spent by concurrent NativeIO operations.
  if (!capacity_tracker_->ChangeAvailableCapacity(requested_difference)) {
    blink::RejectNativeIOWithError(
        resolver, mojom::blink::NativeIOError::New(
                      mojom::blink::NativeIOErrorType::kNoSpace,
                      "Attempted to release more capacity than available."));
    return;
  }

  backend_->RequestCapacityChange(
      requested_difference,
      WTF::BindOnce(&NativeIOFileManager::OnRequestCapacityChangeResult,
                    WrapPersistent(this), WrapPersistent(resolver)));
}

void NativeIOFileManager::GetRemainingCapacityImpl(
    ScriptPromiseResolver* resolver) {
  DCHECK(storage_access_allowed_.has_value())
      << "called without checking if storage access was allowed";
  DCHECK(storage_access_allowed_.value())
      << "called even though storage access was denied";

  uint64_t available_capacity =
      base::as_unsigned(capacity_tracker_->GetAvailableCapacity());
  resolver->Resolve(available_capacity);
}

void NativeIOFileManager::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  visitor->Trace(capacity_tracker_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
