// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/discoverable_credential_metadata.h"

namespace device {

DiscoverableCredentialMetadata::DiscoverableCredentialMetadata(
    std::vector<uint8_t> cred_id_in,
    PublicKeyCredentialUserEntity user_in)
    : cred_id(std::move(cred_id_in)), user(std::move(user_in)) {}

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

}  // namespace device
