// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_H_

#include "base/containers/flat_map.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Implements the storage interface with an in-memory store.
class CONTENT_EXPORT AggregationServiceStorage
    : public AggregationServiceKeyStorage {
 public:
  AggregationServiceStorage();
  AggregationServiceStorage(const AggregationServiceStorage& other) = delete;
  AggregationServiceStorage& operator=(const AggregationServiceStorage& other) =
      delete;
  ~AggregationServiceStorage() override;

  // AggregationServiceKeyStorage:
  PublicKeysForOrigin GetPublicKeys(const url::Origin& origin) const override;
  void SetPublicKeys(const PublicKeysForOrigin& keys) override;
  void ClearPublicKeys(const url::Origin& origin) override;

 private:
  base::flat_map<url::Origin, std::vector<PublicKey>> public_keys_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_H_
