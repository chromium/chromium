// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::SubAppsServiceListResultPtr;
using mojom::blink::SubAppsServiceResult;

namespace {

// We get called back from the SubAppsService mojo service (inside the browser
// process), pass on the result to the calling context.
void OnAddSubApp(ScriptPromiseResolver* resolver, SubAppsServiceResult result) {
  DCHECK(resolver);
  ScriptState* resolver_script_state = resolver->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }
  ScriptState::Scope script_state_scope(resolver_script_state);
  if (result == SubAppsServiceResult::kSuccess) {
    resolver->Resolve();
  } else {
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver_script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "Unable to add given sub-app."));
  }
}

void OnListSubApp(ScriptPromiseResolver* resolver,
                  SubAppsServiceListResultPtr result) {
  DCHECK(resolver);
  ScriptState* resolver_script_state = resolver->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }
  ScriptState::Scope script_state_scope(resolver_script_state);
  if (result->code == SubAppsServiceResult::kSuccess) {
    resolver->Resolve(result->sub_app_ids);
  } else {
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver_script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "Unable to list sub-apps."));
  }
}

}  // namespace

// static
const char SubApps::kSupplementName[] = "SubApps";

SubApps::SubApps(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      service_(navigator.GetExecutionContext()) {}

// static
SubApps* SubApps::subApps(Navigator& navigator) {
  SubApps* subapps = Supplement<Navigator>::From<SubApps>(navigator);
  if (!subapps) {
    subapps = MakeGarbageCollected<SubApps>(navigator);
    ProvideTo(navigator, subapps);
  }
  return subapps;
}

void SubApps::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
  visitor->Trace(service_);
}

HeapMojoRemote<mojom::blink::SubAppsService>& SubApps::GetService() {
  if (!service_.is_bound()) {
    auto* context = GetSupplementable()->GetExecutionContext();
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // In case the other endpoint gets disconnected, we want to reset our end of
    // the pipe as well so that we don't remain connected to a half-open pipe.
    service_.set_disconnect_handler(
        WTF::Bind(&SubApps::OnConnectionError, WrapWeakPersistent(this)));
  }
  return service_;
}

void SubApps::OnConnectionError() {
  service_.reset();
}

ScriptPromise SubApps::add(ScriptState* script_state,
                           const String& install_url,
                           ExceptionState& exception_state) {
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());

  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  Navigator* const navigator = GetSupplementable();
  const SecurityOrigin* frame_origin = navigator->DomWindow()
                                           ->GetFrame()
                                           ->GetSecurityContext()
                                           ->GetSecurityOrigin();
  KURL completed_url = navigator->DomWindow()->CompleteURL(install_url);
  scoped_refptr<const SecurityOrigin> completed_url_origin =
      SecurityOrigin::Create(completed_url);

  if (!frame_origin->IsSameOriginWith(completed_url_origin.get())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kURLMismatchError,
        "API argument must be a relative path or a fully qualified URL matching"
        " the origin of the caller.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->Add(completed_url.GetPath(),
                    WTF::Bind(&OnAddSubApp, WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise SubApps::list(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetService()->List(WTF::Bind(&OnListSubApp, WrapPersistent(resolver)));

  return resolver->Promise();
}

bool SubApps::CheckPreconditionsMaybeThrow(ExceptionState& exception_state) {
  Navigator* const navigator = GetSupplementable();

  if (!navigator->DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The object is no longer associated to a document.");
    return false;
  }

  if (!navigator->DomWindow()->GetFrame()->IsMainFrame() ||
      navigator->DomWindow()->GetFrame()->GetPage()->IsPrerendering() ||
      navigator->DomWindow()->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in primary top-level browsing contexts.");
    return false;
  }

  return true;
}

}  // namespace blink
