// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cross_origin_storage/cross_origin_storage_manager.h"

#include <utility>

#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cross_origin_storage_get_file_handle_hash.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cross_origin_storage_get_file_handle_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Matches the maximum target origins list length required by the spec and
// enforced by the browser process (`cos_constants::kMaxTargetOriginsLength`).
// https://wicg.github.io/cross-origin-storage/#validate-a-cos-request
constexpr wtf_size_t kMaxTargetOriginsLength = 100;

// The spec requires a COS hash value to match /^[0-9a-f]{64}$/ for SHA-256.
// Lowercase is normative rather than cosmetic: hash values are compared with
// an exact string comparison (only `algorithm` is matched case-insensitively),
// so an uppercase value is rejected rather than normalized.
// https://wicg.github.io/cross-origin-storage/#validate-a-cos-request
bool IsLowercaseHexDigest(const String& str, wtf_size_t length) {
  if (str.length() != length) {
    return false;
  }
  for (wtf_size_t i = 0; i < str.length(); ++i) {
    if (!IsAsciiDigit(str[i]) && (str[i] < 'a' || str[i] > 'f')) {
      return false;
    }
  }
  return true;
}

}  // namespace

const char CrossOriginStorageManager::kSupplementName[] =
    "CrossOriginStorageManager";

// static
CrossOriginStorageManager* CrossOriginStorageManager::crossOriginStorage(
    NavigatorBase& navigator) {
  auto* supplement =
      Supplement<NavigatorBase>::From<CrossOriginStorageManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrossOriginStorageManager>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

CrossOriginStorageManager::CrossOriginStorageManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      service_(navigator.GetExecutionContext()) {}

ScriptPromise<FileSystemFileHandle> CrossOriginStorageManager::getFileHandle(
    ScriptState* script_state,
    const CrossOriginStorageGetFileHandleHash* hash,
    const CrossOriginStorageGetFileHandleOptions* options,
    ExceptionState& exception_state) {
  auto* context = GetSupplementable()->GetExecutionContext();
  if (!context || context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is detached.");
    return EmptyPromise();
  }

  if (context->GetSecurityOrigin()->IsOpaque()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Cross-Origin Storage is not available in opaque origins.");
    return EmptyPromise();
  }

  if (!hash || !hash->hasAlgorithm() || !hash->hasValue()) {
    exception_state.ThrowTypeError(
        "Algorithm and value are required for hash.");
    return EmptyPromise();
  }

  String algorithm = hash->algorithm();
  if (!EqualIgnoringAsciiCase(algorithm, "SHA-256")) {
    exception_state.ThrowTypeError(
        StrCat({"Unsupported hash algorithm: '", algorithm,
                "'. Only SHA-256 is supported."}));
    return EmptyPromise();
  }

  String value = hash->value();
  if (!IsLowercaseHexDigest(value, 64)) {
    exception_state.ThrowTypeError(
        "Invalid SHA-256 hash value: must be 64 lowercase hexadecimal "
        "characters.");
    return EmptyPromise();
  }

  const bool create = options && options->create();
  mojom::blink::CrossOriginStorageOriginsPtr create_origins;

  // Per spec (`validate-a-cos-request`), `options.origins` is validated
  // whenever present (`hasOrigins()`), regardless of `options.create`.
  if (options && options->hasOrigins()) {
    const auto* origins_union = options->origins();
    if (origins_union->IsString()) {
      String str = origins_union->GetAsString();
      if (str != "*") {
        // Spec step 3.1: A single string other than "*" is treated as a list
        // of one candidate URL and validated with the basic URL parser.
        KURL parsed_url(str);
        scoped_refptr<const SecurityOrigin> origin =
            SecurityOrigin::Create(parsed_url);
        if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHttpFamily() ||
            !origin || origin->IsOpaque()) {
          exception_state.ThrowTypeError(
              StrCat({"Invalid origin string: '", str, "'."}));
          return EmptyPromise();
        }
        if (create) {
          Vector<scoped_refptr<const SecurityOrigin>> origins;
          origins.push_back(std::move(origin));
          create_origins = mojom::blink::CrossOriginStorageOrigins::NewOrigins(
              std::move(origins));
        }
      } else if (create) {
        create_origins = mojom::blink::CrossOriginStorageOrigins::NewAnyOrigin(
            mojom::blink::CrossOriginStorageAnyOrigin::New());
      }
    } else if (origins_union->IsStringSequence()) {
      const Vector<String>& origin_strings =
          origins_union->GetAsStringSequence();
      if (origin_strings.size() > kMaxTargetOriginsLength) {
        exception_state.ThrowTypeError(
            "origins sequence exceeds maximum length of 100.");
        return EmptyPromise();
      }
      Vector<scoped_refptr<const SecurityOrigin>> origins;
      origins.reserve(origin_strings.size());
      for (const String& origin_str : origin_strings) {
        KURL parsed_url(origin_str);
        scoped_refptr<const SecurityOrigin> origin =
            SecurityOrigin::Create(parsed_url);
        if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHttpFamily() ||
            !origin || origin->IsOpaque()) {
          exception_state.ThrowTypeError(StrCat(
              {"Invalid origin in origins sequence: '", origin_str, "'."}));
          return EmptyPromise();
        }
        origins.push_back(std::move(origin));
      }
      if (create) {
        create_origins = mojom::blink::CrossOriginStorageOrigins::NewOrigins(
            std::move(origins));
      }
    }
  } else if (create) {
    // When `origins` is omitted on a create request, default to an empty list
    // (restricting access to storing and same-site origins).
    create_origins = mojom::blink::CrossOriginStorageOrigins::NewOrigins({});
  }

  auto* service = GetService();
  if (!service) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Service unavailable.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<FileSystemFileHandle>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  pending_resolvers_.insert(resolver);

  auto mojom_hash = mojom::blink::CrossOriginStorageHash::New(
      mojom::blink::CrossOriginStorageAlgorithm::kSha256, value);

  service->GetFileHandle(
      std::move(mojom_hash), std::move(create_origins),
      blink::BindOnce(&CrossOriginStorageManager::OnGetFileHandleComplete,
                      WrapPersistent(this), WrapPersistent(resolver), value));

  return promise;
}

void CrossOriginStorageManager::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(pending_resolvers_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

mojom::blink::CrossOriginStorageManager*
CrossOriginStorageManager::GetService() {
  if (!service_.is_bound()) {
    auto* context = GetSupplementable()->GetExecutionContext();
    if (!context || context->IsContextDestroyed()) {
      return nullptr;
    }
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    service_.set_disconnect_handler(
        blink::BindOnce(&CrossOriginStorageManager::OnConnectionError,
                        WrapWeakPersistent(this)));
  }
  return service_.get();
}

void CrossOriginStorageManager::OnConnectionError() {
  service_.reset();
  HeapHashSet<Member<ScriptPromiseResolverBase>> resolvers;
  resolvers.swap(pending_resolvers_);
  for (auto& resolver : resolvers) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kAbortError,
        "Cross-Origin Storage service disconnected.");
  }
}

void CrossOriginStorageManager::OnGetFileHandleComplete(
    ScriptPromiseResolver<FileSystemFileHandle>* resolver,
    const String& name,
    mojom::blink::FileSystemAccessErrorPtr result,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle> file_handle) {
  pending_resolvers_.erase(resolver);

  if (!result) {
    resolver->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                     "The operation was aborted.");
    return;
  }

  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    file_system_access_error::Reject(resolver, *result);
    return;
  }

  auto* context = GetSupplementable()->GetExecutionContext();
  if (!context || context->IsContextDestroyed() || !file_handle.is_valid()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Invalid file handle returned.");
    return;
  }

  resolver->Resolve(MakeGarbageCollected<FileSystemFileHandle>(
      context, name, std::move(file_handle)));
}

}  // namespace blink
