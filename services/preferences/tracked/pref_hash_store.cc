// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_store.h"

#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"

// Define the default implementation for the one-argument compatibility
// overload of BeginTransaction.
std::unique_ptr<PrefHashStoreTransaction> PrefHashStore::BeginTransaction(
    HashStoreContents* storage) {
  // Call the primary two-argument overload defined in the interface,
  // explicitly passing nullptr for the encryptor pointer. This ensures code
  // calling the older signature still works and receives a transaction that
  // operates without encryption capabilities enabled for its scope.
  return BeginTransaction(storage, nullptr);
}
