// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file_manager.h"

#include <algorithm>
#include <utility>

#include "base/files/file.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "third_party/blink/renderer/modules/native_io/native_io_error.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file_sync.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
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
  if (name.IsEmpty())
    return false;

  if (name.length() > kMaximumFilenameLength)
    return false;

  if (name.Is8Bit()) {
    return std::all_of(name.Span8().begin(), name.Span8().end(),
                       &IsValidNativeIONameCharacter);
  }
  return std::all_of(name.Span16().begin(), name.Span16().end(),
                     &IsValidNativeIONameCharacter);
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
  ScriptState::Scope scope(script_state);

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
  backend_.set_disconnect_handler(WTF::Bind(
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

  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file(
      execution_context);
  mojo::PendingReceiver<mojom::blink::NativeIOFileHost> backend_file_receiver =
      backend_file.BindNewPipeAndPassReceiver(receiver_task_runner_);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->OpenFile(
      name, std::move(backend_file_receiver),
      WTF::Bind(&NativeIOFileManager::OnOpenResult, WrapPersistent(this),
                WrapPersistent(resolver),
                WrapPersistent(WrapDisallowNew(std::move(backend_file)))));
  return resolver->Promise();
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
  backend_->DeleteFile(
      name, WTF::Bind(&NativeIOFileManager::OnDeleteResult,
                      WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
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
  backend_->GetAllFileNames(
      WTF::Bind(&OnGetAllResult, WrapPersistent(resolver)));
  return resolver->Promise();
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
  backend_->RenameFile(old_name, new_name,
                       WTF::Bind(&OnRenameResult, WrapPersistent(resolver)));
  return resolver->Promise();
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
      std::move(backing_file), std::move(backend_file), execution_context);
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

  mojom::blink::NativeIOErrorPtr delete_error;
  uint64_t deleted_file_size;
  bool call_succeeded =
      backend_->DeleteFile(name, &delete_error, &deleted_file_size);

  if (delete_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    ThrowNativeIOWithError(exception_state, std::move(delete_error));
    return;
  }
  DCHECK(call_succeeded) << "Mojo call failed";
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
  backend_->RequestCapacityChange(
      requested_capacity,
      WTF::Bind(&NativeIOFileManager::OnRequestCapacityChangeResult,
                WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
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
  if (!base::IsValueInRangeForNumericType<int64_t>(requested_release)) {
    ThrowNativeIOWithError(
        exception_state,
        mojom::blink::NativeIOError::New(
            mojom::blink::NativeIOErrorType::kNoSpace,
            "Attempted to release more capacity than available"));
    return ScriptPromise();
  }

  int64_t requested_difference = -base::as_signed(requested_release);

  // Reducing available capacity must be done before performing the IPC, so
  // capacity cannot be double-spent by concurrent NativeIO operations.
  if (!capacity_tracker_->ChangeAvailableCapacity(requested_difference)) {
    ThrowNativeIOWithError(
        exception_state,
        mojom::blink::NativeIOError::New(
            mojom::blink::NativeIOErrorType::kNoSpace,
            "Attempted to release more capacity than available."));
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->RequestCapacityChange(
      -requested_release,
      WTF::Bind(&NativeIOFileManager::OnRequestCapacityChangeResult,
                WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise NativeIOFileManager::getRemainingCapacity(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // TODO(rstz): Consider using ScriptPromise::Cast instead.
  const ScriptPromise promise = resolver->Promise();
  uint64_t available_capacity =
      base::as_unsigned(capacity_tracker_->GetAvailableCapacity());
  resolver->Resolve(available_capacity);
  return promise;
}

void NativeIOFileManager::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  visitor->Trace(capacity_tracker_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
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
  ScriptState::Scope scope(script_state);

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
  ScriptState::Scope scope(script_state);
  // If `granted_capacity` < 0, the available capacity has already been released
  // prior to the IPC.
  if (granted_capacity > 0) {
    capacity_tracker_->ChangeAvailableCapacity(
        base::as_signed(granted_capacity));
  }
  uint64_t available_capacity = capacity_tracker_->GetAvailableCapacity();

  resolver->Resolve(available_capacity);
}

}  // namespace blink
