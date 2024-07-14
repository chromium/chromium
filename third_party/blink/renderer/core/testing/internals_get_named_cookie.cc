// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/internals_get_named_cookie.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/cookie_manager/cookie_manager_automation.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_cookie.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_cookie_same_site.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/internals_cookies.h"

namespace blink {

// static
ScriptPromise<IDLNullable<InternalCookie>>
InternalsGetNamedCookie::getNamedCookie(ScriptState* script_state,
                                        Internals&,
                                        const String& name) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  mojo::Remote<test::mojom::blink::CookieManagerAutomation> cookie_manager;
  window->GetBrowserInterfaceBroker().GetInterface(
      cookie_manager.BindNewPipeAndPassReceiver());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<InternalCookie>>>(
          script_state);
  auto promise = resolver->Promise();
  // Get the interface so `cookie_manager` can be moved below.
  test::mojom::blink::CookieManagerAutomation* raw_cookie_manager =
      cookie_manager.get();
  raw_cookie_manager->GetNamedCookie(
      name, WTF::BindOnce(
                [](ScriptPromiseResolver<IDLNullable<InternalCookie>>* resolver,
                   ScriptState* script_state,
                   mojo::Remote<test::mojom::blink::CookieManagerAutomation>,
                   network::mojom::blink::CookieWithAccessResultPtr cookie) {
                  InternalCookie* result = nullptr;
                  if (cookie) {
                    result = CookieMojomToInternalCookie(
                        cookie, script_state->GetIsolate());
                  }
                  resolver->Resolve(result);
                },
                WrapPersistent(resolver), WrapPersistent(script_state),
                // Keep `cookie_manager` alive to wait for callback.
                std::move(cookie_manager)));
  return promise;
}

}  // namespace blink
