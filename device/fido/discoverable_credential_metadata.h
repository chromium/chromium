// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_user_entity.h"

namespace device {

// DiscoverableCredentialMetadata contains information about a credential that
// may be available silently. Specifically, the credential ID and user
// information.
class COMPONENT_EXPORT(DEVICE_FIDO) DiscoverableCredentialMetadata {
 public:
  DiscoverableCredentialMetadata(AuthenticatorType source,
                                 std::string rp_id,
                                 std::vector<uint8_t> cred_id,
                                 PublicKeyCredentialUserEntity user,
                                 std::optional<std::string> provider_name,
                                 std::optional<base::Time> last_used_time = base::Time());

  DiscoverableCredentialMetadata();
  DiscoverableCredentialMetadata(const DiscoverableCredentialMetadata& other);
  DiscoverableCredentialMetadata(DiscoverableCredentialMetadata&& other);
  DiscoverableCredentialMetadata& operator=(
      const DiscoverableCredentialMetadata& other);
  DiscoverableCredentialMetadata& operator=(
      DiscoverableCredentialMetadata&& other);
  ~DiscoverableCredentialMetadata();
  bool operator==(const DiscoverableCredentialMetadata& other) const;

  AuthenticatorType source = AuthenticatorType::kOther;
  std::string rp_id;
  std::vector<uint8_t> cred_id;
  PublicKeyCredentialUserEntity user;
  // system_created is set to true for credentials that were created
  // automatically by the system. This can happen on Windows where (at least) a
  // credential for login.microsoft.com can be auto-created for users.
  bool system_created = false;

  // The localized display name of the credential provider reported by the OS
  // API. On macOS 14+, AuthenticationServices populates this for *all*
  // credentials, including Apple's built-in manager ("Passwords" on macOS 15+,
  // "iCloud Keychain" on macOS 14) as well as third-party credential provider
  // extensions ("1Password", "Bitwarden", etc.). On Windows (webauthn.dll v8+),
  // this is populated only for third-party plugin authenticators and is nullopt
  // for built-in Windows Hello.
  std::optional<std::string> provider_name;

  // True when the credential originated from Apple Passwords, as opposed to
  // another provider. This affects what icon will be shown for this credential
  // in UI.
  bool is_apple_passwords = false;

  // The transports that this credential supports, as reported by the platform
  // API. May be empty if the platform does not provide transport information.
  base::flat_set<FidoTransportProtocol> transports;

  // Time when this passkey was last successfully asserted. Can be nullopt for
  // platform provided credentials.
  std::optional<base::Time> last_used_time;
};

}  // namespace device

#endif  // DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_
