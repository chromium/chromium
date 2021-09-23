// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_STORAGE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_STORAGE_H_

#include <vector>

#include "content/common/content_export.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

struct PublicKey;
struct PublicKeyset;

// This class provides an interface for persisting helper server public keys
// and performing queries on it.
class CONTENT_EXPORT AggregationServiceKeyStorage {
 public:
  virtual ~AggregationServiceKeyStorage() = default;

  // Returns the public keys for `origin` that are currently valid. The returned
  // value should not be stored for future operations as it may expire soon.
  virtual std::vector<PublicKey> GetPublicKeys(const url::Origin& origin) = 0;

  // Sets the public keys for `origin`.
  virtual void SetPublicKeys(const url::Origin& origin,
                             const PublicKeyset& keyset) = 0;

  // Clears the stored public keys for `origin`.
  virtual void ClearPublicKeys(const url::Origin& origin) = 0;

  // Clears the stored public keys that were fetched between `delete_begin` and
  // `delete_end` time (inclusive). Null times are treated as unbounded lower or
  // upper range.
  virtual void ClearPublicKeysFetchedBetween(base::Time delete_begin,
                                             base::Time delete_end) = 0;

  // Clears the stored public keys that expire no later than `delete_end`
  // (inclusive).
  virtual void ClearPublicKeysExpiredBy(base::Time delete_end) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_STORAGE_H_
