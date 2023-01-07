// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MOJOM_TRAITS_H_

#include "device/fido/fido_constants.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::test::mojom::ClientToAuthenticatorProtocol,
               device::ProtocolVersion> {
  static blink::test::mojom::ClientToAuthenticatorProtocol ToMojom(
      device::ProtocolVersion input);
  static bool FromMojom(blink::test::mojom::ClientToAuthenticatorProtocol input,
                        device::ProtocolVersion* output);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::test::mojom::Ctap2Version, device::Ctap2Version> {
  static blink::test::mojom::Ctap2Version ToMojom(device::Ctap2Version input);
  static bool FromMojom(blink::test::mojom::Ctap2Version input,
                        device::Ctap2Version* output);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_AUTHENTICATOR_MOJOM_TRAITS_H_
