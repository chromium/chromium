// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_manager.h"

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

bool IsValidNativeIOName(const String& name) {
  if (name.IsEmpty())
    return false;

  if (name.Is8Bit()) {
    return std::all_of(name.Span8().begin(), name.Span8().end(),
                       &IsValidNativeIONameCharacter);
  }
  return std::all_of(name.Span16().begin(), name.Span16().end(),
                     &IsValidNativeIONameCharacter);
}

void OnOpenResult(
    ScriptPromiseResolver* resolver,
    DisallowNewWrapper<HeapMojoRemote<mojom::blink::NativeIOFileHost>>*
        backend_file_wrapper,
    base::File backing_file) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!backing_file.IsValid()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        "open() failed"));
    return;
  }

  NativeIOFile* file = MakeGarbageCollected<NativeIOFile>(
      std::move(backing_file), backend_file_wrapper->TakeValue(),
      ExecutionContext::From(script_state));
  resolver->Resolve(file);
}

void OnDeleteResult(ScriptPromiseResolver* resolver, bool backend_success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!backend_success) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        "delete() failed"));
    return;
  }

  resolver->Resolve();
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

void OnRenameResult(ScriptPromiseResolver* resolver, bool backend_success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!backend_success) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        "rename() failed"));
    return;
  }
  resolver->Resolve();
}

}  // namespace

NativeIOManager::NativeIOManager(
    ExecutionContext* execution_context,
    HeapMojoRemote<mojom::blink::NativeIOHost> backend)
    : ExecutionContextClient(execution_context),
      // TODO(pwnall): Get a dedicated queue when the specification matures.
      receiver_task_runner_(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      backend_(std::move(backend)) {
  backend_.set_disconnect_handler(WTF::Bind(
      &NativeIOManager::OnBackendDisconnect, WrapWeakPersistent(this)));
}

NativeIOManager::~NativeIOManager() = default;

ScriptPromise NativeIOManager::open(ScriptState* script_state,
                                    String name,
                                    ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return ScriptPromise();
  }

  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
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
      WTF::Bind(&OnOpenResult, WrapPersistent(resolver),
                WrapPersistent(WrapDisallowNew(std::move(backend_file)))));
  return resolver->Promise();
}

ScriptPromise NativeIOManager::Delete(ScriptState* script_state,
                                      String name,
                                      ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return ScriptPromise();
  }

  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->DeleteFile(name,
                       WTF::Bind(&OnDeleteResult, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise NativeIOManager::getAll(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->GetAllFileNames(
      WTF::Bind(&OnGetAllResult, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise NativeIOManager::rename(ScriptState* script_state,
                                      String old_name,
                                      String new_name,
                                      ExceptionState& exception_state) {
  if (!IsValidNativeIOName(old_name) || !IsValidNativeIOName(new_name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return ScriptPromise();
  }

  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  backend_->RenameFile(old_name, new_name,
                       WTF::Bind(&OnRenameResult, WrapPersistent(resolver)));
  return resolver->Promise();
}

NativeIOFileSync* NativeIOManager::openSync(String name,
                                            ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return nullptr;
  }

  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return nullptr;
  }

  ExecutionContext* execution_context = GetExecutionContext();
  DCHECK(execution_context);

  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file(
      execution_context);
  mojo::PendingReceiver<mojom::blink::NativeIOFileHost> backend_file_receiver =
      backend_file.BindNewPipeAndPassReceiver(receiver_task_runner_);

  base::File backing_file;
  bool call_succeeded =
      backend_->OpenFile(name, std::move(backend_file_receiver), &backing_file);

  if (!call_succeeded || !backing_file.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "openSync() failed");
    return nullptr;
  }

  return MakeGarbageCollected<NativeIOFileSync>(
      std::move(backing_file), std::move(backend_file), execution_context);
}

void NativeIOManager::deleteSync(String name, ExceptionState& exception_state) {
  if (!IsValidNativeIOName(name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return;
  }

  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return;
  }

  bool backend_success = false;
  bool call_succeeded = backend_->DeleteFile(name, &backend_success);

  if (!call_succeeded || !backend_success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "deleteSync() failed");
  }
}

Vector<String> NativeIOManager::getAllSync(ExceptionState& exception_state) {
  Vector<String> result;
  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return result;
  }

  bool backend_success = false;
  bool call_succeeded = backend_->GetAllFileNames(&backend_success, &result);

  if (!call_succeeded || !backend_success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "getAllSync() failed");
  }
  return result;
}

void NativeIOManager::renameSync(String old_name,
                                 String new_name,
                                 ExceptionState& exception_state) {
  if (!IsValidNativeIOName(old_name) || !IsValidNativeIOName(new_name)) {
    exception_state.ThrowTypeError("Invalid file name");
    return;
  }

  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "NativeIOHost backend went away");
    return;
  }

  bool backend_success = false;
  bool call_succeeded =
      backend_->RenameFile(old_name, new_name, &backend_success);

  if (!call_succeeded || !backend_success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "renameSync() failed");
  }
}

void NativeIOManager::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void NativeIOManager::OnBackendDisconnect() {
  backend_.reset();
}

}  // namespace blink
