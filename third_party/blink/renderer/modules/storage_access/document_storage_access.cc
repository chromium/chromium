// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/document_storage_access.h"

#include "base/metrics/histogram_functions.h"
#include "net/storage_access_api/status.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_access_types.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/cookie_jar.h"
#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

namespace blink {

namespace {

// This enum must match the numbering for RequestStorageResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// at the end.
enum class RequestStorageResult {
  APPROVED_EXISTING_ACCESS = 0,
  // APPROVED_NEW_GRANT = 1,
  REJECTED_NO_USER_GESTURE = 2,
  REJECTED_NO_ORIGIN = 3,
  REJECTED_OPAQUE_ORIGIN = 4,
  REJECTED_EXISTING_DENIAL = 5,
  REJECTED_SANDBOXED = 6,
  REJECTED_GRANT_DENIED = 7,
  REJECTED_INCORRECT_FRAME = 8,
  REJECTED_INSECURE_CONTEXT = 9,
  APPROVED_PRIMARY_FRAME = 10,
  REJECTED_CREDENTIALLESS_IFRAME = 11,
  APPROVED_NEW_OR_EXISTING_GRANT = 12,
  REJECTED_FENCED_FRAME = 13,
  REJECTED_INVALID_ORIGIN = 14,
  kMaxValue = REJECTED_INVALID_ORIGIN,
};

void FireRequestStorageAccessHistogram(RequestStorageResult result) {
  base::UmaHistogramEnumeration("API.StorageAccess.RequestStorageAccess2",
                                result);
}

void FireRequestStorageAccessForHistogram(RequestStorageResult result) {
  base::UmaHistogramEnumeration(
      "API.TopLevelStorageAccess.RequestStorageAccessFor2", result);
}

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

// static
ScriptPromise<IDLUndefined> DocumentStorageAccess::requestStorageAccessFor(
    ScriptState* script_state,
    Document& document,
    const AtomicString& site) {
  return From(document).requestStorageAccessFor(script_state, site);
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
    if (!GetSupplementable()->dom_window_->isSecureContext()) {
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
      WTF::BindOnce([](ScriptPromiseResolver<IDLUndefined>* resolver) {
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
      WTF::BindOnce(
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
    FireRequestStorageAccessHistogram(RequestStorageResult::REJECTED_NO_ORIGIN);

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

  if (!GetSupplementable()->dom_window_->isSecureContext()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: May not be used in an insecure context."));
    FireRequestStorageAccessHistogram(
        RequestStorageResult::REJECTED_INSECURE_CONTEXT);

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }

  if (GetSupplementable()->IsInOutermostMainFrame()) {
    FireRequestStorageAccessHistogram(
        RequestStorageResult::APPROVED_PRIMARY_FRAME);

    // If this is the outermost frame we no longer need to make a request and
    // can resolve the promise.
    resolver->Resolve();
    return promise;
  }

  if (GetSupplementable()->dom_window_->GetSecurityOrigin()->IsOpaque()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: Cannot be used by opaque origins."));
    FireRequestStorageAccessHistogram(
        RequestStorageResult::REJECTED_OPAQUE_ORIGIN);

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }

  if (GetSupplementable()->dom_window_->credentialless()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: May not be used in a credentialless iframe"));
    FireRequestStorageAccessHistogram(
        RequestStorageResult::REJECTED_CREDENTIALLESS_IFRAME);

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

    FireRequestStorageAccessHistogram(
        GetSupplementable()->dom_window_->GetFrame()->IsInFencedFrameTree()
            ? RequestStorageResult::REJECTED_FENCED_FRAME
            : RequestStorageResult::REJECTED_SANDBOXED);

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
    return promise;
  }
  if (RuntimeEnabledFeatures::FedCmWithStorageAccessAPIEnabled(
          GetSupplementable()->GetExecutionContext()) &&
      GetSupplementable()->GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kIdentityCredentialsGet)) {
    UseCounter::Count(GetSupplementable()->GetExecutionContext(),
                      WebFeature::kFedCmWithStorageAccessAPI);
  }
  // RequestPermission may return `GRANTED` without actually creating a
  // permission grant if cookies are already accessible.
  auto descriptor = mojom::blink::PermissionDescriptor::New();
  descriptor->name = mojom::blink::PermissionName::STORAGE_ACCESS;
  GetSupplementable()
      ->GetPermissionService(ExecutionContext::From(resolver->GetScriptState()))
      ->RequestPermission(
          std::move(descriptor),
          LocalFrame::HasTransientUserActivation(
              GetSupplementable()->GetFrame()),
          WTF::BindOnce(
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
    mojom::blink::PermissionStatus status) {
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

  if (status == mojom::blink::PermissionStatus::GRANTED) {
    FireRequestStorageAccessHistogram(
        RequestStorageResult::APPROVED_NEW_OR_EXISTING_GRANT);
    if (request_unpartitioned_cookie_access) {
      GetSupplementable()->dom_window_->SetStorageAccessApiStatus(
          net::StorageAccessApiStatus::kAccessViaAPI);
    }
    std::move(on_resolve).Run(resolver);
  } else {
    LocalFrame::ConsumeTransientUserActivation(GetSupplementable()->GetFrame());
    FireRequestStorageAccessHistogram(
        RequestStorageResult::REJECTED_GRANT_DENIED);
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccess: Permission denied."));
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccess not allowed"));
  }
}

ScriptPromise<IDLUndefined> DocumentStorageAccess::requestStorageAccessFor(
    ScriptState* script_state,
    const AtomicString& origin) {
  if (!GetSupplementable()->GetFrame()) {
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_NO_ORIGIN);
    // Note that in detached frames, resolvers are not able to return a promise.
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "requestStorageAccessFor: Cannot be used unless "
                          "the document is fully active."));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);

  // Access the promise first to ensure it is created so that the proper state
  // can be changed when it is resolved or rejected.
  auto promise = resolver->Promise();

  if (!GetSupplementable()->IsInOutermostMainFrame()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccessFor: Only supported in primary top-level "
        "browsing contexts."));
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_INCORRECT_FRAME);
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccessFor not allowed"));
    return promise;
  }

  if (GetSupplementable()->dom_window_->GetSecurityOrigin()->IsOpaque()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccessFor: Cannot be used by opaque origins."));

    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_OPAQUE_ORIGIN);
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccessFor not allowed"));
    return promise;
  }

  // `requestStorageAccessFor` must be rejected for any given iframe. In
  // particular, it must have been rejected by credentialless iframes:
  CHECK(!GetSupplementable()->dom_window_->credentialless());

  if (!GetSupplementable()->dom_window_->isSecureContext()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccessFor: May not be used in an insecure "
        "context."));
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_INSECURE_CONTEXT);

    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccessFor not allowed"));
    return promise;
  }

  KURL origin_as_kurl{origin};
  if (!origin_as_kurl.IsValid()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccessFor: Invalid origin."));
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_INVALID_ORIGIN);
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Invalid origin"));
    return promise;
  }

  scoped_refptr<SecurityOrigin> supplied_origin =
      SecurityOrigin::Create(origin_as_kurl);
  if (supplied_origin->IsOpaque()) {
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccessFor: Invalid origin parameter."));
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_OPAQUE_ORIGIN);
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccessFor not allowed"));
    return promise;
  }

  if (GetSupplementable()->dom_window_->GetSecurityOrigin()->IsSameSiteWith(
          supplied_origin.get())) {
    // Access is not actually disabled, so accept the request.
    resolver->Resolve();
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::APPROVED_EXISTING_ACCESS);
    return promise;
  }

  auto descriptor = mojom::blink::PermissionDescriptor::New();
  descriptor->name = mojom::blink::PermissionName::TOP_LEVEL_STORAGE_ACCESS;
  auto top_level_storage_access_extension =
      mojom::blink::TopLevelStorageAccessPermissionDescriptor::New();
  top_level_storage_access_extension->requestedOrigin = supplied_origin;
  descriptor->extension =
      mojom::blink::PermissionDescriptorExtension::NewTopLevelStorageAccess(
          std::move(top_level_storage_access_extension));

  GetSupplementable()
      ->GetPermissionService(ExecutionContext::From(script_state))
      ->RequestPermission(
          std::move(descriptor),
          LocalFrame::HasTransientUserActivation(
              GetSupplementable()->GetFrame()),
          WTF::BindOnce(&DocumentStorageAccess::
                            ProcessTopLevelStorageAccessPermissionState,
                        WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void DocumentStorageAccess::ProcessTopLevelStorageAccessPermissionState(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::blink::PermissionStatus status) {
  DCHECK(resolver);
  DCHECK(GetSupplementable()->GetFrame());
  ScriptState* script_state = resolver->GetScriptState();
  DCHECK(script_state);
  ScriptState::Scope scope(script_state);

  if (status == mojom::blink::PermissionStatus::GRANTED) {
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::APPROVED_NEW_OR_EXISTING_GRANT);
    resolver->Resolve();
  } else {
    LocalFrame::ConsumeTransientUserActivation(GetSupplementable()->GetFrame());
    FireRequestStorageAccessForHistogram(
        RequestStorageResult::REJECTED_GRANT_DENIED);
    GetSupplementable()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kSecurity, ConsoleMessage::Level::kError,
        "requestStorageAccessFor: Permission denied."));
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
        "requestStorageAccessFor not allowed"));
  }
}

}  // namespace blink
