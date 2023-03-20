// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/discoverable_credential_metadata.h"

namespace device {

DiscoverableCredentialMetadata::DiscoverableCredentialMetadata(
    AuthenticatorType source_in,
    std::string rp_id_in,
    std::vector<uint8_t> cred_id_in,
    PublicKeyCredentialUserEntity user_in)
    : source(source_in),
      rp_id(std::move(rp_id_in)),
      cred_id(std::move(cred_id_in)),
      user(std::move(user_in)) {}

DiscoverableCredentialMetadata::DiscoverableCredentialMetadata() = default;
DiscoverableCredentialMetadata::DiscoverableCredentialMetadata(
    const DiscoverableCredentialMetadata& other) = default;
DiscoverableCredentialMetadata::DiscoverableCredentialMetadata(
    DiscoverableCredentialMetadata&& other) = default;
DiscoverableCredentialMetadata& DiscoverableCredentialMetadata::operator=(
    const DiscoverableCredentialMetadata& other) = default;
DiscoverableCredentialMetadata& DiscoverableCredentialMetadata::operator=(
    DiscoverableCredentialMetadata&& other) = default;
DiscoverableCredentialMetadata::~DiscoverableCredentialMetadata() = default;

bool DiscoverableCredentialMetadata::operator==(
    const DiscoverableCredentialMetadata& other) const {
  return rp_id == other.rp_id && cred_id == other.cred_id && user == other.user;
}

}  // namespace device
