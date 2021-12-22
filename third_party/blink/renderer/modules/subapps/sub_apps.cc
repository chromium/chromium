// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/subapps/sub_apps.h"

#include <utility>

#include "base/check.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_provider.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String SubAppsProviderResultToString(
    mojom::blink::SubAppsProviderResult value) {
  switch (value) {
    case mojom::blink::SubAppsProviderResult::kSuccess:
      return "success";
    case mojom::blink::SubAppsProviderResult::kFailure:
      return "failure";
  }
}

// We get called back from the SubAppsProvider mojo service (inside the browser
// process), pass on the result to the calling context.
void OnAddSubApp(ScriptPromiseResolver* resolver,
                 mojo::Remote<mojom::blink::SubAppsProvider> /* ignored */,
                 mojom::blink::SubAppsProviderResult result) {
  if (result == mojom::blink::SubAppsProviderResult::kSuccess) {
    resolver->Resolve(SubAppsProviderResultToString(result));
  } else {
    resolver->Reject(SubAppsProviderResultToString(result));
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

ScriptPromise SubApps::add(ScriptState* script_state,
                           const String& install_url) {
  Navigator* const navigator = GetSupplementable();
  // [SecureContext] from the IDL ensures this.
  DCHECK(ExecutionContext::From(script_state)->IsSecureContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  if (!navigator->DomWindow()) {
    auto* exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated to a document.");
    resolver->Reject(exception);
    return resolver->Promise();
  }

  if (!navigator->DomWindow()->GetFrame()->IsMainFrame()) {
    auto* exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in top-level browsing contexts.");
    resolver->Reject(exception);
    return resolver->Promise();
  }

  KURL completed_url = KURL(navigator->DomWindow()->Url(), install_url);
  if (!url::Origin::Create(navigator->DomWindow()->Url())
           .IsSameOriginWith(url::Origin::Create(completed_url))) {
    auto* exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "API argument must be a relative path or a fully qualified URL matching"
        " the origin of the caller.");
    resolver->Reject(exception);
    return resolver->Promise();
  }

  mojo::Remote<mojom::blink::SubAppsProvider> provider;
  ExecutionContext::From(script_state)
      ->GetBrowserInterfaceBroker()
      .GetInterface(provider.BindNewPipeAndPassReceiver());

  auto* raw_provider = provider.get();
  raw_provider->Add(
      completed_url.GetPath(),
      WTF::Bind(&OnAddSubApp, WrapPersistent(resolver), std::move(provider)));

  return resolver->Promise();
}

}  // namespace blink
