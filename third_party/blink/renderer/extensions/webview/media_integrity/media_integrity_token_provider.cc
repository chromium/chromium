// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_token_provider.h"

#include "third_party/blink/public/mojom/webview/webview_media_integrity.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_webview/v8/v8_media_integrity_error_name.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {
const char kInvalidContext[] = "Invalid context";
}  // namespace

namespace blink {

MediaIntegrityTokenProvider::MediaIntegrityTokenProvider(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::WebViewMediaIntegrityProvider>
        provider_pending_remote,
    uint64_t cloud_project_number)
    : provider_remote_(context), cloud_project_number_(cloud_project_number) {
  provider_remote_.Bind(std::move(provider_pending_remote),
                        context->GetTaskRunner(TaskType::kInternalDefault));
  provider_remote_.set_disconnect_handler(
      WTF::BindOnce(&MediaIntegrityTokenProvider::OnProviderConnectionError,
                    WrapWeakPersistent(this)));
}

void MediaIntegrityTokenProvider::OnProviderConnectionError() {
  provider_remote_.reset();
  for (auto& resolver : token_resolvers_) {
    ScriptState* script_state = resolver->GetScriptState();
    if (!script_state->ContextIsValid()) {
      continue;
    }
    ScriptState::Scope scope(script_state);
    resolver->Reject(MediaIntegrityError::CreateForName(
        script_state->GetIsolate(),
        V8MediaIntegrityErrorName::Enum::kTokenProviderInvalid));
  }
  token_resolvers_.clear();
}

ScriptPromise<IDLString> MediaIntegrityTokenProvider::requestToken(
    ScriptState* script_state,
    const String& opt_content_binding,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidContext);
    return EmptyPromise();
  }
  ScriptState::Scope scope(script_state);

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
          script_state, exception_state.GetContext());
  ScriptPromise<IDLString> promise = resolver->Promise();

  if (!provider_remote_.is_bound()) {
    // We cannot reconnect ourselves. The caller must request a new provider.
    resolver->Reject(MediaIntegrityError::CreateForName(
        script_state->GetIsolate(),
        V8MediaIntegrityErrorName::Enum::kTokenProviderInvalid));
    return promise;
  }

  token_resolvers_.insert(resolver);
  provider_remote_->RequestToken(
      opt_content_binding,
      WTF::BindOnce(&MediaIntegrityTokenProvider::OnRequestTokenResponse,
                    WrapPersistent(this), WrapPersistent(script_state),
                    WrapPersistent(resolver)));
  return promise;
}

void MediaIntegrityTokenProvider::OnRequestTokenResponse(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLString>* resolver,
    const mojom::blink::WebViewMediaIntegrityTokenResponsePtr response) {
  token_resolvers_.erase(resolver);

  if (!script_state->ContextIsValid()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidContext));
    return;
  }
  ScriptState::Scope scope(script_state);

  if (response->is_token()) {
    resolver->Resolve(response->get_token());
  } else {
    const mojom::blink::WebViewMediaIntegrityErrorCode error_code =
        response->get_error_code();
    resolver->Reject(MediaIntegrityError::CreateFromMojomEnum(
        script_state->GetIsolate(), error_code));
  }
}

void MediaIntegrityTokenProvider::Trace(Visitor* visitor) const {
  visitor->Trace(token_resolvers_);
  visitor->Trace(provider_remote_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
