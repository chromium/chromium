// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_authenticator_mojom_traits.h"  // nogncheck

namespace mojo {
using blink::test::mojom::ClientToAuthenticatorProtocol;
using blink::test::mojom::Ctap2Version;

// static
ClientToAuthenticatorProtocol
EnumTraits<ClientToAuthenticatorProtocol, device::ProtocolVersion>::ToMojom(
    device::ProtocolVersion input) {
  switch (input) {
    case ::device::ProtocolVersion::kU2f:
      return ClientToAuthenticatorProtocol::U2F;
    case ::device::ProtocolVersion::kCtap2:
      return ClientToAuthenticatorProtocol::CTAP2;
    case ::device::ProtocolVersion::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return ClientToAuthenticatorProtocol::U2F;
  }
  NOTREACHED_IN_MIGRATION();
  return ClientToAuthenticatorProtocol::U2F;
}

// static
bool EnumTraits<ClientToAuthenticatorProtocol, device::ProtocolVersion>::
    FromMojom(ClientToAuthenticatorProtocol input,
              device::ProtocolVersion* output) {
  switch (input) {
    case ClientToAuthenticatorProtocol::U2F:
      *output = ::device::ProtocolVersion::kU2f;
      return true;
    case ClientToAuthenticatorProtocol::CTAP2:
      *output = ::device::ProtocolVersion::kCtap2;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
Ctap2Version EnumTraits<Ctap2Version, device::Ctap2Version>::ToMojom(
    device::Ctap2Version input) {
  switch (input) {
    case ::device::Ctap2Version::kCtap2_0:
      return Ctap2Version::CTAP2_0;
    case ::device::Ctap2Version::kCtap2_1:
      return Ctap2Version::CTAP2_1;
  }
  NOTREACHED_IN_MIGRATION();
  return Ctap2Version::CTAP2_0;
}

// static
bool EnumTraits<Ctap2Version, device::Ctap2Version>::FromMojom(
    Ctap2Version input,
    device::Ctap2Version* output) {
  switch (input) {
    case Ctap2Version::CTAP2_0:
      *output = ::device::Ctap2Version::kCtap2_0;
      return true;
    case Ctap2Version::CTAP2_1:
      *output = ::device::Ctap2Version::kCtap2_1;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
