// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CDM_STORAGE_DATA_MODEL_H_
#define CONTENT_PUBLIC_BROWSER_CDM_STORAGE_DATA_MODEL_H_

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using CdmStorageKeyUsageSize =
    std::vector<std::pair<blink::StorageKey, uint64_t>>;

namespace content {

// To integrate the CdmStorage data type with the BrowsingDataModel, the
// interface CdmStorageDataModel is created to expose the two functions to
// populate the BrowsingDataModel with:
//  1. Method to obtain disk usage of data stored by the CDM (Content Decryption
//     Module) per storage key.
//  2. Method to delete the CDM data for a storage key.
class CONTENT_EXPORT CdmStorageDataModel {
 public:
  virtual ~CdmStorageDataModel() = default;

  virtual void GetUsagePerAllStorageKeys(
      base::OnceCallback<void(const CdmStorageKeyUsageSize&)> callback,
      base::Time begin,
      base::Time end) = 0;

  virtual void DeleteDataForStorageKey(
      const blink::StorageKey& storage_key,
      base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CDM_STORAGE_DATA_MODEL_H_
