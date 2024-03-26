// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/document_storage_access.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_access_types.h"
#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

namespace blink {

namespace {

class RequestExtendedStorageAccess final : public ScriptFunction::Callable {
 public:
  RequestExtendedStorageAccess(
      LocalDOMWindow& window,
      const StorageAccessTypes* storage_access_types,
      ScriptPromiseResolver<StorageAccessHandle>* resolver)
      : window_(&window),
        storage_access_types_(storage_access_types),
        resolver_(resolver) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue) override {
    resolver_->Resolve(MakeGarbageCollected<StorageAccessHandle>(
        *window_, storage_access_types_));
    return ScriptValue();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(window_);
    visitor->Trace(storage_access_types_);
    visitor->Trace(resolver_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  Member<LocalDOMWindow> window_;
  Member<const StorageAccessTypes> storage_access_types_;
  Member<ScriptPromiseResolver<StorageAccessHandle>> resolver_;
};

}  // namespace

// static
const char DocumentStorageAccess::kNoAccessRequested[] =
    "You must request access for at least one storage/communication medium.";

// static
const char DocumentStorageAccess::kSupplementName[] = "DocumentStorageAccess";

// static
DocumentStorageAccess& DocumentStorageAccess::From(Document& document) {
  DocumentStorageAccess* supplement =
      Supplement<Document>::From<DocumentStorageAccess>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentStorageAccess>(document);
    ProvideTo(document, supplement);
  }
  return *supplement;
}

// static
ScriptPromise<StorageAccessHandle> DocumentStorageAccess::requestStorageAccess(
    ScriptState* script_state,
    Document& document,
    const StorageAccessTypes* storage_access_types) {
  return From(document).requestStorageAccess(script_state,
                                             storage_access_types);
}

// static
ScriptPromise<IDLBoolean> DocumentStorageAccess::hasUnpartitionedCookieAccess(
    ScriptState* script_state,
    Document& document) {
  return From(document).hasUnpartitionedCookieAccess(script_state);
}

DocumentStorageAccess::DocumentStorageAccess(Document& document)
    : Supplement<Document>(document) {}

void DocumentStorageAccess::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

ScriptPromise<StorageAccessHandle> DocumentStorageAccess::requestStorageAccess(
    ScriptState* script_state,
    const StorageAccessTypes* storage_access_types) {
  if (!storage_access_types->all() && !storage_access_types->cookies() &&
      !storage_access_types->sessionStorage() &&
      !storage_access_types->localStorage() &&
      !storage_access_types->indexedDB() && !storage_access_types->locks() &&
      !storage_access_types->caches() &&
      !storage_access_types->getDirectory() &&
      !storage_access_types->estimate() &&
      !storage_access_types->createObjectURL() &&
      !storage_access_types->revokeObjectURL() &&
      !storage_access_types->broadcastChannel() &&
      !storage_access_types->sharedWorker()) {
    return ScriptPromise<StorageAccessHandle>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kSecurityError,
                          DocumentStorageAccess::kNoAccessRequested));
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<StorageAccessHandle>>(
          script_state);
  auto promise = resolver->Promise();
  GetSupplementable()
      ->RequestStorageAccessImpl(
          script_state,
          storage_access_types->all() || storage_access_types->cookies())
      .Then(MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RequestExtendedStorageAccess>(
                            *GetSupplementable()->domWindow(),
                            storage_access_types, resolver)));
  return promise;
}

ScriptPromise<IDLBoolean> DocumentStorageAccess::hasUnpartitionedCookieAccess(
    ScriptState* script_state) {
  return GetSupplementable()->hasStorageAccess(script_state);
}

}  // namespace blink
