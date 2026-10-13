// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/document_storage_access.h"

#include "net/storage_access_api/status.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_access_types.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/cookie_jar.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

namespace blink {

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
ScriptPromise<IDLBoolean> DocumentStorageAccess::hasStorageAccess(
    ScriptState* script_state,
    Document& document) {
  return From(document).hasStorageAccess(script_state);
}

// static
ScriptPromise<IDLUndefined> DocumentStorageAccess::requestStorageAccess(
    ScriptState* script_state,
    Document& document) {
  return From(document).requestStorageAccess(script_state);
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

ScriptPromise<IDLBoolean> DocumentStorageAccess::hasStorageAccess(
    ScriptState* script_state) {
  // See
  // https://privacycg.github.io/storage-access/#dom-document-hasstorageaccess
  // for the steps implemented here.

  // Step #2: if doc is not fully active, reject p with an InvalidStateError and
  // return p.
  if (!GetSupplementable()->GetFrame()) {
    // Note that in detached frames, resolvers are not able to return a promise.
    return ScriptPromise<IDLBoolean>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "hasStorageAccess: Cannot be used unless the "
                          "document is fully active."));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve([&]() -> bool {
    // #3: if doc's origin is opaque, return false.
    if (GetSupplementable()
            ->GetExecutionContext()
            ->GetSecurityOrigin()
            ->IsOpaque()) {
      return false;
    }

    // #?: if window.credentialless is true, return false.
    if (GetSupplementable()->dom_window_->credentialless()) {
      return false;
    }

    // #5: if global is not a secure context, return false.
    if (!GetSupplementable()->dom_window_->IsSecureContext()) {
      return false;
    }

    // #6: if the top-level origin of doc's relevant settings object is an
    // opaque origin, return false.
    if (GetSupplementable()->TopFrameOrigin()->IsOpaque()) {
      return false;
    }

    // #7 - #10: checks unpartitioned cookie availability with global's `has
    // storage access`.
    return GetSupplementable()->CookiesEnabled();
  }());
  return promise;
}

ScriptPromise<IDLUndefined> DocumentStorageAccess::requestStorageAccess(
    ScriptState* script_state) {
  // Requesting storage access via `requestStorageAccess()` idl always requests
  // unpartitioned cookie access.
  return RequestStorageAccessImpl(
      script_state,
      /*request_unpartitioned_cookie_access=*/true,
      BindOnce([](ScriptPromiseResolver<IDLUndefined>* resolver) {
        DCHECK(resolver);
        resolver->Resolve();
      }));
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
  return RequestStorageAccessImpl(
      script_state,
      /*request_unpartitioned_cookie_access=*/storage_access_types->all() ||
          storage_access_types->cookies(),
      BindOnce(
          [](LocalDOMWindow* window,
             const StorageAccessTypes* storage_access_types,
             ScriptPromiseResolver<StorageAccessHandle>* resolver) {
            if (!window) {
              return;
            }
            DCHECK(storage_access_types);
            DCHECK(resolver);
            resolver->Resolve(MakeGarbageCollected<StorageAccessHandle>(
                *window, storage_access_types));
          },
          WrapWeakPersistent(GetSupplementable()->domWindow()),
          WrapPersistent(storage_access_types)));
}

ScriptPromise<IDLBoolean> DocumentStorageAccess::hasUnpartitionedCookieAccess(
    ScriptState* script_state) {
  return hasStorageAccess(script_state);
}

template <typename T>
ScriptPromise<T> DocumentStorageAccess::RequestStorageAccessImpl(
    ScriptState* script_state,
    bool request_unpartitioned_cookie_access,
    base::OnceCallback<void(ScriptPromiseResolver<T>*)> on_resolve) {
  if (!GetSupplementable()->GetFrame()) {
    // Note that in detached frames, resolvers are not able to return a promise.
    return ScriptPromise<T>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "requestStorageAccess: Cannot be used unless the "
                          "document is fully active."));
  }

  if (GetSupplementable()->cookie_jar_) {
    // Storage access might be about to change in which case the ability for
    // |cookie_jar_| to retrieve values might also. Invalidate its cache in case
    // that happens so it can't return data that shouldn't be accessible.
    GetSupplementable()->cookie_jar_->InvalidateCache();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<T>>(script_state);

  // Access the promise first to ensure it is created so that the proper state
  // can be changed when it is resolved or rejected.
  auto promise = resolver->Promise();

  if (!GetSupplementable()->dom_window_->IsSecureContext()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: May not be used in an insecure context."));

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }

  // If this is the outermost frame we no longer need to make a request and
  // can resolve the promise.
  if (GetSupplementable()->IsInOutermostMainFrame()) {
    resolver->Resolve();
    return promise;
  }

  if (GetSupplementable()->dom_window_->GetSecurityOrigin()->IsOpaque()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: Cannot be used by opaque origins."));

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }

  if (GetSupplementable()->dom_window_->credentialless()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: May not be used in a credentialless iframe"));

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }

  if (GetSupplementable()->dom_window_->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::
              kStorageAccessByUserActivation)) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        GetSupplementable()->dom_window_->GetFrame()->IsInFencedFrameTree()
            ? "requestStorageAccess: Refused to execute request. The document "
              "is in a fenced frame tree."
            : "requestStorageAccess: Refused to execute request. The document "
              "is sandboxed, and the 'allow-storage-access-by-user-activation' "
              "keyword is not set."));


    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }
  if (GetSupplementable()->GetExecutionContext()->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    UseCounter::Count(GetSupplementable()->GetExecutionContext(),
                      WebFeature::kFedCmWithStorageAccessAPI);
  }

  // All reasons why the storage key might forbid unpartitioned storage access
  // should have been covered above. If this check fails, a feature must have
  // been added without adding a new check above.
  CHECK(!GetSupplementable()
             ->dom_window_->GetStorageKey()
             .ForbidsUnpartitionedStorageAccess());

  // RequestPermission may return `GRANTED` without actually creating a
  // permission grant if cookies are already accessible.
  auto descriptor = mojom::blink::PermissionDescriptor::New();
  descriptor->name = mojom::blink::PermissionName::STORAGE_ACCESS;
  GetSupplementable()
      ->GetPermissionService(ExecutionContext::From(resolver->GetScriptState()))
      ->RequestPermission(
          std::move(descriptor),
          blink::BindOnce(
              &DocumentStorageAccess::ProcessStorageAccessPermissionState<T>,
              WrapPersistent(this), WrapPersistent(resolver),
              request_unpartitioned_cookie_access, std::move(on_resolve)));

  return promise;
}

template <typename T>
void DocumentStorageAccess::ProcessStorageAccessPermissionState(
    ScriptPromiseResolver<T>* resolver,
    bool request_unpartitioned_cookie_access,
    base::OnceCallback<void(ScriptPromiseResolver<T>*)> on_resolve,
    mojom::blink::PermissionStatusWithDetailsPtr status) {
  DCHECK(resolver);

  ScriptState* script_state = resolver->GetScriptState();
  DCHECK(script_state);
  ScriptState::Scope scope(script_state);

  // document could be no longer alive.
  if (!GetSupplementable()->dom_window_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "document shutdown"));
    return;
  }

  if (status->status == mojom::blink::PermissionStatus::GRANTED) {
    if (request_unpartitioned_cookie_access) {
      GetSupplementable()->dom_window_->SetStorageAccessApiStatus(
          net::StorageAccessApiStatus::kAccessViaAPI);
    }
    std::move(on_resolve).Run(resolver);
  } else {
    LocalFrame::ConsumeTransientUserActivation(GetSupplementable()->GetFrame());
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: Permission denied."));
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
  }
}

}  // namespace blink
