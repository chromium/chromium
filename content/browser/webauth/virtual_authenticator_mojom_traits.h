// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MOJOM_TRAITS_H_

#include "device/fido/fido_constants.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

namespace mojo {

using blink::test::mojom::ClientToAuthenticatorProtocol;

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<ClientToAuthenticatorProtocol, device::ProtocolVersion> {
  static ClientToAuthenticatorProtocol ToMojom(device::ProtocolVersion input);
  static bool FromMojom(ClientToAuthenticatorProtocol input,
                        device::ProtocolVersion* output);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MOJOM_TRAITS_H_
