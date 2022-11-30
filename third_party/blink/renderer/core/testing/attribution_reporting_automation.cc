// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/attribution_reporting_automation.h"

#include <utility>

#include "base/check.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/conversions/attribution_reporting_automation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
ScriptPromise AttributionReportingAutomation::resetAttributionReporting(
    ScriptState* script_state,
    Internals&) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  mojo::Remote<test::mojom::blink::AttributionReportingAutomation> automation;
  window->GetBrowserInterfaceBroker().GetInterface(
      automation.BindNewPipeAndPassReceiver());
  DCHECK(automation.is_bound());

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  test::mojom::blink::AttributionReportingAutomation* raw_automation =
      automation.get();

  raw_automation->Reset(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver,
         mojo::Remote<test::mojom::blink::AttributionReportingAutomation>,
         bool success) {
        if (success) {
          resolver->Resolve();
        } else {
          resolver->Reject();
        }
      },
      WrapPersistent(resolver),
      // Keep `automation` alive to wait for callback.
      std::move(automation)));

  return promise;
}

}  // namespace blink
