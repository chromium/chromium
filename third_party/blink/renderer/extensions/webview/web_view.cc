// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/web_view.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_webview/v8/v8_get_media_integrity_token_provider_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_token_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

const char WebView::kSupplementName[] = "WebView";

WebView& WebView::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());

  auto* supplement =
      Supplement<ExecutionContext>::From<WebView>(execution_context);

  if (!supplement) {
    supplement = MakeGarbageCollected<WebView>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

WebView::WebView(ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context),
      ExecutionContextClient(&execution_context) {}

ScriptPromise WebView::getExperimentalMediaIntegrityTokenProvider(
    ScriptState* script_state,
    GetMediaIntegrityTokenProviderParams* params,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid context");
    return ScriptPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  // TODO(crbug.com/327186031): Communicate with browser to instantiate a proper
  // token provider and bind the handle here.
  MediaIntegrityTokenProvider* provider =
      MakeGarbageCollected<MediaIntegrityTokenProvider>(
          ExecutionContext::From(script_state), params->cloudProjectNumber());

  resolver->Resolve(provider);
  return promise;
}

void WebView::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
