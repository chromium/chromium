// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_federated_credential_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {
constexpr char kFederatedCredentialType[] = "federated";
}  // namespace

FederatedCredential* FederatedCredential::Create(
    const FederatedCredentialInit* data,
    ExceptionState& exception_state) {
  if (data->id().empty()) {
    exception_state.ThrowTypeError("'id' must not be empty.");
    return nullptr;
  }
  if (data->provider().empty()) {
    exception_state.ThrowTypeError("'provider' must not be empty.");
    return nullptr;
  }

  KURL icon_url;
  if (data->hasIconURL())
    icon_url = ParseStringAsURLOrThrow(data->iconURL(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  KURL provider_url =
      ParseStringAsURLOrThrow(data->provider(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  String name;
  if (data->hasName())
    name = data->name();

  return MakeGarbageCollected<FederatedCredential>(
      data->id(), SecurityOrigin::Create(provider_url), name, icon_url);
}

FederatedCredential* FederatedCredential::Create(
    const String& id,
    scoped_refptr<const SecurityOrigin> provider,
    const String& name,
    const KURL& icon_url) {
  return MakeGarbageCollected<FederatedCredential>(
      id, provider, name, icon_url.IsEmpty() ? blink::KURL() : icon_url);
}

FederatedCredential::FederatedCredential(
    const String& id,
    scoped_refptr<const SecurityOrigin> provider_origin,
    const String& name,
    const KURL& icon_url)
    : Credential(id, kFederatedCredentialType),
      provider_origin_(provider_origin),
      name_(name),
      icon_url_(icon_url) {
  DCHECK(provider_origin);
}

bool FederatedCredential::IsFederatedCredential() const {
  return true;
}

void SetIdpSigninStatus(const blink::LocalFrameToken& local_frame_token,
                        const url::Origin& origin,
                        mojom::blink::IdpSigninStatus status) {
  CHECK(WTF::IsMainThread());
  LocalFrame* local_frame = LocalFrame::FromFrameToken(local_frame_token);
  if (!local_frame) {
    return;
  }
  auto* auth_request = CredentialManagerProxy::From(local_frame->DomWindow())
                           ->FederatedAuthRequest();
  auth_request->SetIdpSigninStatus(SecurityOrigin::CreateFromUrlOrigin(origin),
                                   status);
}

}  // namespace blink
