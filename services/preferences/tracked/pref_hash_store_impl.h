// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_IMPL_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/values.h"
#include "services/preferences/tracked/pref_hash_calculator.h"
#include "services/preferences/tracked/pref_hash_store.h"

// Implements PrefHashStoreImpl by storing preference hashes in a
// HashStoreContents.
class PrefHashStoreImpl : public PrefHashStore {
 public:
  enum StoreVersion {
    // No hashes have been stored in this PrefHashStore yet.
    VERSION_UNINITIALIZED = 0,
    // The hashes in this PrefHashStore were stored before the introduction
    // of a version number and should be re-initialized.
    VERSION_PRE_MIGRATION = 1,
    // The hashes in this PrefHashStore were stored using the latest algorithm.
    VERSION_LATEST = 2,
  };

  using PrefHashStore::BeginTransaction;

  // Constructs a PrefHashStoreImpl that calculates hashes using `seed`.
  //
  // The same `seed` must be used to load and validate previously stored hashes.
  //
  // `use_super_mac` controls whether a Super MAC is calculated and verified.
  // `use_super_encrypted_hash` controls whether a Super Encrypted Hash is
  // verified on startup.
  PrefHashStoreImpl(const std::string& seed,
                    bool use_super_mac,
                    bool use_super_encrypted_hash);

  PrefHashStoreImpl(const PrefHashStoreImpl&) = delete;
  PrefHashStoreImpl& operator=(const PrefHashStoreImpl&) = delete;

  ~PrefHashStoreImpl() override;

  // Clears the contents of this PrefHashStore. |IsInitialized()| will return
  // false after this call.
  void Reset();

  // PrefHashStore implementation.
  std::unique_ptr<PrefHashStoreTransaction> BeginTransaction(
      HashStoreContents* storage,
      const os_crypt_async::Encryptor* encryptor) override;

  std::string ComputeMac(const std::string& path,
                         const base::Value* new_value) override;
  base::DictValue ComputeSplitMacs(
      const std::string& path,
      const base::DictValue* split_values) override;

  std::string ComputeEncryptedHash(
      const std::string& path,
      const base::Value* value,
      const os_crypt_async::Encryptor* encryptor) override;

  std::string ComputeEncryptedHash(
      const std::string& path,
      const base::DictValue* dict,
      const os_crypt_async::Encryptor* encryptor) override;

  base::DictValue ComputeSplitEncryptedHashes(
      const std::string& path,
      const base::DictValue* split_values,
      const os_crypt_async::Encryptor* encryptor) override;

 private:
  friend class PrefHashStoreImplTest;
  friend class PrefHashStoreImplEncryptedTest;
  class PrefHashStoreTransactionImpl;

  std::string ComputeMac(const std::string& path,
                         const base::DictValue* new_dict);

  static void FilterEncryptedHashesRecursive(const base::DictValue& src,
                                             base::DictValue& dest);

  const PrefHashCalculator pref_hash_calculator_;
  bool use_super_mac_;
  bool use_super_encrypted_hash_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_IMPL_H_
