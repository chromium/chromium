// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_

#include <vector>

#include "base/component_export.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

// DiscoverableCredentialMetadata contains information about a credential that
// may be available silently. Specifically, the credential ID and user
// information.
class COMPONENT_EXPORT(DEVICE_FIDO) DiscoverableCredentialMetadata {
 public:
  DiscoverableCredentialMetadata(AuthenticatorType source,
                                 std::string rp_id,
                                 std::vector<uint8_t> cred_id,
                                 PublicKeyCredentialUserEntity user);

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
};

}  // namespace device

#endif  // DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_
