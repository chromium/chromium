// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/testing/internals_protected_audience.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/test/mojom/privacy_sandbox/web_privacy_sandbox_automation.test-mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

// static
ScriptPromise<IDLUndefined>
InternalsProtectedAudience::setProtectedAudienceKAnonymity(
    ScriptState* script_state,
    Internals&,
    const String& owner_origin_str,
    const String& name,
    const Vector<String>& hashes_base64) {
  const ExecutionContext* context = ExecutionContext::From(script_state);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  scoped_refptr<const SecurityOrigin> owner_origin =
      SecurityOrigin::CreateFromString(owner_origin_str);
  if (!owner_origin->Host().EndsWith(".test")) {
    resolver->Reject("owner origin must be on a .test domain");
    return promise;
  }

  for (const auto& in_hash_b64 : hashes_base64) {
    Vector<uint8_t> hash;
    if (!Base64Decode(in_hash_b64, hash, Base64DecodePolicy::kForgiving)) {
      resolver->Reject("hash not base64 encoded");
      return promise;
    }
  }

  mojo::Remote<test::mojom::blink::WebPrivacySandboxAutomation>
      sandbox_automation;
  context->GetBrowserInterfaceBroker().GetInterface(
      sandbox_automation.BindNewPipeAndPassReceiver());
  auto* raw_sandbox_automation = sandbox_automation.get();
  raw_sandbox_automation->SetProtectedAudienceKAnonymity(
      owner_origin, name, hashes_base64,
      BindOnce(
          // The remote is taken to take its ownership.
          [](ScriptPromiseResolver<IDLUndefined>* resolver,
             mojo::Remote<test::mojom::blink::WebPrivacySandboxAutomation>) {
            resolver->Resolve();
          },
          WrapPersistent(resolver), std::move(sandbox_automation)));
  return promise;
}

}  // namespace blink
