// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_

#include <vector>

#include "base/component_export.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

// DiscoverableCredentialMetadata contains information about a credential that
// may be available silently. Specifically, the credential ID and user
// information.
class COMPONENT_EXPORT(DEVICE_FIDO) DiscoverableCredentialMetadata {
 public:
  DiscoverableCredentialMetadata(std::vector<uint8_t> cred_id,
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

  std::vector<uint8_t> cred_id;
  PublicKeyCredentialUserEntity user;
};

}  // namespace device

#endif  // DEVICE_FIDO_DISCOVERABLE_CREDENTIAL_METADATA_H_
