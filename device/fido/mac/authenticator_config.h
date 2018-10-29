// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_AUTHENTICATOR_CONFIG_H_
#define DEVICE_FIDO_MAC_AUTHENTICATOR_CONFIG_H_

#include <string>

#include "base/component_export.h"

namespace device {
namespace fido {
namespace mac {

struct COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorConfig {
  // The keychain-access-group value used for WebAuthn credentials
  // stored in the macOS keychain by the built-in Touch ID
  // authenticator.
  std::string keychain_access_group;
  // The secret used to derive key material when encrypting WebAuthn
  // credential metadata for storage in the macOS keychain. Chrome returns
  // different secrets for each user profile in order to logically separate
  // credentials per profile.
  std::string metadata_secret;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_AUTHENTICATOR_CONFIG_H_
