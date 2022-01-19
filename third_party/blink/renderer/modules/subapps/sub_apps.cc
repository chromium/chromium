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
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated to a document.");
    return ScriptPromise();
  }

  if (!navigator->DomWindow()->GetFrame()->IsMainFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in top-level browsing contexts.");
    return ScriptPromise();
  }

  KURL completed_url = KURL(navigator->DomWindow()->Url(), install_url);
  if (!url::IsSameOriginWith(navigator->DomWindow()->Url(), completed_url)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
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
