// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/testing/internals_storage_access.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_automation.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// static
ScriptPromise<IDLUndefined> InternalsStorageAccess::setStorageAccess(
    ScriptState* script_state,
    Internals&,
    const String& origin,
    const String& embedding_origin,
    const bool blocked,
    ExceptionState& exception_state) {
  mojo::Remote<test::mojom::blink::StorageAccessAutomation>
      storage_access_automation;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      storage_access_automation.BindNewPipeAndPassReceiver());
  DCHECK(storage_access_automation.is_bound());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* raw_storage_access_automation = storage_access_automation.get();
  raw_storage_access_automation->SetStorageAccess(
      origin, embedding_origin, blocked,
      WTF::BindOnce(
          // While we only really need |resolver|, we also take the
          // mojo::Remote<> so that it remains alive after this function exits.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::StorageAccessAutomation>,
             bool success) {
            if (success)
              resolver->Resolve();
            else
              resolver->Reject();
          },
          WrapPersistent(resolver), std::move(storage_access_automation)));

  return promise;
}

}  // namespace blink
