// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "services/preferences/tracked/pref_hash_calculator.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"

class HashStoreContents;
class PrefHashStoreTransaction;

// Holds the configuration and implementation used to calculate and verify
// preference MACs.
// TODO(gab): Rename this class as it is no longer a store.
class PrefHashStore {
 public:
  virtual ~PrefHashStore() {}

  // Returns a PrefHashStoreTransaction which can be used to perform a series
  // of operations on the hash store. |storage| MAY be used as the backing store
  // depending on the implementation. Therefore the HashStoreContents used for
  // related transactions should correspond to the same underlying data store.
  // |storage| must outlive the returned transaction.
  virtual std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      HashStoreContents* storage,
      const os_crypt_async::Encryptor* encryptor) = 0;
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      HashStoreContents* storage);

  // Computes the MAC to be associated with |path| and |value| in this store.
  // PrefHashStoreTransaction typically uses this internally but it's also
  // exposed for users that want to compute MACs ahead of time for asynchronous
  // operations.
  virtual std::string ComputeMac(const std::string& path,
                                 const base::Value* value) = 0;

  // Computes the MAC to be associated with |path| and |split_values| in this
  // store. PrefHashStoreTransaction typically uses this internally but it's
  // also exposed for users that want to compute MACs ahead of time for
  // asynchronous operations.
  virtual base::Value::Dict ComputeSplitMacs(
      const std::string& path,
      const base::Value::Dict* split_values) = 0;

  // Computes the OS-encrypted hash for a given path and value.
  // Requires a non-null |encryptor|.
  virtual std::string ComputeEncryptedHash(
      const std::string& path,
      const base::Value* value,
      const os_crypt_async::Encryptor* encryptor) = 0;

  // Computes the OS-encrypted hash for a given path and dictionary value.
  // Requires a non-null |encryptor|.
  virtual std::string ComputeEncryptedHash(
      const std::string& path,
      const base::Value::Dict* dict,
      const os_crypt_async::Encryptor* encryptor) = 0;

  // Computes the OS-encrypted hashes for a dictionary split across
  // multiple keys. Requires a non-null |encryptor|.
  virtual base::Value::Dict ComputeSplitEncryptedHashes(
      const std::string& path,
      const base::Value::Dict* split_values,
      const os_crypt_async::Encryptor* encryptor) = 0;
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_H_
