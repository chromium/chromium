// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webid/federated_identity_data_model.h"

#include <utility>

#include "base/check.h"

namespace webid {

FederatedIdentityDataModel::DataKey::DataKey(
    url::Origin relying_party_requester,
    url::Origin relying_party_embedder,
    url::Origin identity_provider,
    std::string account_id)
    : relying_party_requester_(std::move(relying_party_requester)),
      relying_party_embedder_(std::move(relying_party_embedder)),
      identity_provider_(std::move(identity_provider)),
      account_id_(std::move(account_id)) {
  DCHECK(!relying_party_requester_.opaque());
  DCHECK(!relying_party_embedder_.opaque());
  DCHECK(!identity_provider_.opaque());
  DCHECK(!account_id_.empty());
}

FederatedIdentityDataModel::DataKey::DataKey(const DataKey&) = default;

FederatedIdentityDataModel::DataKey::DataKey(DataKey&&) = default;

FederatedIdentityDataModel::DataKey&
FederatedIdentityDataModel::DataKey::operator=(const DataKey&) = default;

FederatedIdentityDataModel::DataKey&
FederatedIdentityDataModel::DataKey::operator=(DataKey&&) = default;

FederatedIdentityDataModel::DataKey::~DataKey() = default;

bool FederatedIdentityDataModel::DataKey::operator<(
    const FederatedIdentityDataModel::DataKey& other) const {
  return std::tie(relying_party_requester_, relying_party_embedder_,
                  identity_provider_, account_id_) <
         std::tie(other.relying_party_requester_, other.relying_party_embedder_,
                  other.identity_provider_, other.account_id_);
}

bool FederatedIdentityDataModel::DataKey::operator==(
    const FederatedIdentityDataModel::DataKey& other) const {
  return std::tie(relying_party_requester_, relying_party_embedder_,
                  identity_provider_, account_id_) ==
         std::tie(other.relying_party_requester_, other.relying_party_embedder_,
                  other.identity_provider_, other.account_id_);
}

}  // namespace webid
