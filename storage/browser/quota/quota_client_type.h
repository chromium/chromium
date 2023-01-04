// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_TYPE_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_TYPE_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"

namespace storage {

// Identifies storage features for browser data deletion.
//
// Values are not expected to be persisted or logged. Enum members may be added,
// removed, and reordered without warning.
enum class QuotaClientType {
  kFileSystem = 1,
  kDatabase = 2,
  kIndexedDatabase = 3,
  kServiceWorkerCache = 4,
  kServiceWorker = 5,
  kBackgroundFetch = 6,
  kMediaLicense = 7,
};

// Set of QuotaClientType values.
//
// TODO(pwnall): Switch to std::bitset, or another type that's cheap to copy.
using QuotaClientTypes = base::flat_set<QuotaClientType>;

COMPONENT_EXPORT(STORAGE_BROWSER) const QuotaClientTypes& AllQuotaClientTypes();

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_CLIENT_TYPE_H_
