// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBID_FEDERATED_IDENTITY_DATA_MODEL_H_
#define COMPONENTS_WEBID_FEDERATED_IDENTITY_DATA_MODEL_H_

#include <set>

#include "base/functional/callback_forward.h"
#include "url/origin.h"

namespace webid {

class FederatedIdentityDataModel {
 public:
  class DataKey {
   public:
    explicit DataKey(url::Origin relying_party_requester,
                     url::Origin relying_party_embedder,
                     url::Origin identity_provider,
                     std::string account_id);

    DataKey(const DataKey&);
    DataKey(DataKey&&);

    DataKey& operator=(const DataKey&);
    DataKey& operator=(DataKey&&);

    ~DataKey();

    const url::Origin& relying_party_requester() const {
      return relying_party_requester_;
    }
    const url::Origin& relying_party_embedder() const {
      return relying_party_embedder_;
    }
    const url::Origin& identity_provider() const { return identity_provider_; }
    const std::string& account_id() const { return account_id_; }

    bool operator<(const DataKey&) const;

    bool operator==(const DataKey&) const;

   private:
    url::Origin relying_party_requester_;
    url::Origin relying_party_embedder_;
    url::Origin identity_provider_;
    std::string account_id_;
  };

  virtual ~FederatedIdentityDataModel() = default;

  virtual void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DataKey>)> callback) = 0;

  virtual void RemoveFederatedIdentityDataByDataKey(
      const DataKey& data_key,
      base::OnceClosure callback) = 0;
};

}  // namespace webid

#endif  // COMPONENTS_WEBID_FEDERATED_IDENTITY_DATA_MODEL_H_
