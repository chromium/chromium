// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/internals_delete_all_cookies.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/cookie_manager/cookie_manager_automation.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// static
ScriptPromise<IDLUndefined> InternalsDeleteAllCookies::deleteAllCookies(
    ScriptState* script_state,
    Internals&) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  mojo::Remote<test::mojom::blink::CookieManagerAutomation> cookie_manager;
  window->GetBrowserInterfaceBroker().GetInterface(
      cookie_manager.BindNewPipeAndPassReceiver());
  DCHECK(cookie_manager.is_bound());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  // Get the interface so `cookie_manager` can be moved below.
  test::mojom::blink::CookieManagerAutomation* raw_cookie_manager =
      cookie_manager.get();
  raw_cookie_manager->DeleteAllCookies(WTF::BindOnce(
      [](ScriptPromiseResolver<IDLUndefined>* resolver,
         mojo::Remote<test::mojom::blink::CookieManagerAutomation>) {
        resolver->Resolve();
      },
      WrapPersistent(resolver),
      // Keep `cookie_manager` alive to wait for callback.
      std::move(cookie_manager)));
  return promise;
}

}  // namespace blink
