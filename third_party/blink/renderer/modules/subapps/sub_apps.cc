// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// We get called back from the SubAppsService mojo service (inside the browser
// process), pass on the result to the calling context.
void OnAddSubApp(ScriptPromiseResolver* resolver,
                 mojom::blink::SubAppsServiceResult result) {
  if (result == mojom::blink::SubAppsServiceResult::kSuccess) {
    resolver->Resolve();
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "Unable to add given sub-app."));
  }
}

}  // namespace

// static
const char SubApps::kSupplementName[] = "SubApps";

SubApps::SubApps(Navigator& navigator) : Supplement<Navigator>(navigator) {}

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
}

mojo::Remote<mojom::blink::SubAppsService>& SubApps::GetService() {
  if (!service_.is_bound()) {
    GetSupplementable()
        ->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(service_.BindNewPipeAndPassReceiver());
    // In case the other endpoint gets disconnected, we want to reset our end of
    // the pipe as well so that we don't remain connected to a half-open pipe.
    service_.reset_on_disconnect();
  }
  return service_;
}

ScriptPromise SubApps::add(ScriptState* script_state,
                           const String& install_url,
                           ExceptionState& exception_state) {
  Navigator* const navigator = GetSupplementable();
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());

  if (!navigator->DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The object is no longer associated to a document.");
    return ScriptPromise();
  }

  if (!navigator->DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in top-level browsing contexts.");
    return ScriptPromise();
  }

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

}  // namespace blink
