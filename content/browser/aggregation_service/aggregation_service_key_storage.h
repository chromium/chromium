// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_STORAGE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_STORAGE_H_

#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// This class provides an interface for persisting helper server public keys
// and performing queries on it.
class CONTENT_EXPORT AggregationServiceKeyStorage {
 public:
  virtual ~AggregationServiceKeyStorage() = default;

  // Returns the public keys for `origin`.
  virtual PublicKeysForOrigin GetPublicKeys(
      const url::Origin& origin) const = 0;

  // Sets the public keys for `origin`.
  virtual void SetPublicKeys(const PublicKeysForOrigin& keys) = 0;

  // Clears the stored public keys for `origin`.
  virtual void ClearPublicKeys(const url::Origin& origin) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_STORAGE_H_