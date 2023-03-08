// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/navigator_installed_app.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ScriptPromise NavigatorInstalledApp::getInstalledRelatedApps(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated to a document.");
    resolver->Reject(exception_state);
    return promise;
  }

  if (!navigator.DomWindow()->GetFrame()->IsOutermostMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "getInstalledRelatedApps() is only supported in "
        "top-level browsing contexts.");
    resolver->Reject(exception_state);
    return promise;
  }

  auto* app_controller = InstalledAppController::From(*navigator.DomWindow());
  app_controller->GetInstalledRelatedApps(
      std::make_unique<
          CallbackPromiseAdapter<HeapVector<Member<RelatedApplication>>, void>>(
          resolver));
  return promise;
}

}  // namespace blink
