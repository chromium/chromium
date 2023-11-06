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

class RequestExtendedStorageAccess final : public ScriptFunction::Callable,
                                           public Supplement<LocalDOMWindow> {
 public:
  explicit RequestExtendedStorageAccess(
      LocalDOMWindow& window,
      const StorageAccessTypes* storage_access_types)
      : Supplement<LocalDOMWindow>(window),
        storage_access_types_(storage_access_types) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue) override {
    ScriptPromiseResolver* resolver =
        MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    ScriptPromise promise = resolver->Promise();
    StorageAccessHandle* handle = MakeGarbageCollected<StorageAccessHandle>(
        *GetSupplementable(), storage_access_types_);
    resolver->Resolve(handle);
    return promise.AsScriptValue();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(storage_access_types_);
    ScriptFunction::Callable::Trace(visitor);
    Supplement<LocalDOMWindow>::Trace(visitor);
  }

 private:
  Member<const StorageAccessTypes> storage_access_types_;
};

}  // namespace

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
ScriptPromise DocumentStorageAccess::requestStorageAccess(
    ScriptState* script_state,
    Document& document,
    const StorageAccessTypes* storage_access_types) {
  return From(document).requestStorageAccess(script_state,
                                             storage_access_types);
}

DocumentStorageAccess::DocumentStorageAccess(Document& document)
    : Supplement<Document>(document) {}

void DocumentStorageAccess::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

ScriptPromise DocumentStorageAccess::requestStorageAccess(
    ScriptState* script_state,
    const StorageAccessTypes* storage_access_types) {
  return GetSupplementable()
      ->requestStorageAccess(script_state)
      .Then(MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<RequestExtendedStorageAccess>(
              *GetSupplementable()->domWindow(), storage_access_types)));
}

}  // namespace blink
