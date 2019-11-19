// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/federated_credential.h"

#include "third_party/blink/renderer/modules/credentialmanager/federated_credential_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
constexpr char kFederatedCredentialType[] = "federated";
}

FederatedCredential* FederatedCredential::Create(
    const FederatedCredentialInit* data,
    ExceptionState& exception_state) {
  if (data->id().IsEmpty())
    exception_state.ThrowTypeError("'id' must not be empty.");
  if (data->provider().IsEmpty())
    exception_state.ThrowTypeError("'provider' must not be empty.");

  KURL icon_url = ParseStringAsURLOrThrow(data->iconURL(), exception_state);
  KURL provider_url =
      ParseStringAsURLOrThrow(data->provider(), exception_state);

  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<FederatedCredential>(
      data->id(), SecurityOrigin::Create(provider_url), data->name(), icon_url);
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
    scoped_refptr<const SecurityOrigin> provider,
    const String& name,
    const KURL& icon_url)
    : Credential(id, kFederatedCredentialType),
      provider_(provider),
      name_(name),
      icon_url_(icon_url) {
  DCHECK(provider);
}

bool FederatedCredential::IsFederatedCredential() const {
  return true;
}

}  // namespace blink
