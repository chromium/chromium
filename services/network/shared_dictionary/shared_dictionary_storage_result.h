// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_RESULT_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_RESULT_H_

namespace network {

// Used for UMA. Logged to "Net.SharedDictionaryOnDisk.StorageResult".
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SharedDictionaryStorageResult)
enum class SharedDictionaryStorageResult {
  kSuccess = 0,
  kErrorCreateEntryFailed = 1,
  kErrorWriteDataFailed = 2,
  kErrorSizeExceedsLimit = 3,
  kErrorSizeZero = 4,
  kErrorAborted = 5,
  kErrorDatabaseWriteFailed = 6,
  kMaxValue = kErrorDatabaseWriteFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SharedDictionaryStorageResult)

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_RESULT_H_
