// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "virtual_authenticator_mojom_traits.h"

namespace mojo {

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
      NOTREACHED();
      return ClientToAuthenticatorProtocol::U2F;
  }
  NOTREACHED();
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
  NOTREACHED();
  return false;
}

}  // namespace mojo
