// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_
#define SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_

#include <string>
#include <string_view>

#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/temp_scoped_dir_cleaner.h"

// Helper object to clear registry entries for scoped temporary pref stores.
class TempScopedDirRegistryCleaner : public TempScopedDirCleaner {
 public:
  void SetRegistryPath(const std::wstring& registry_path);

 private:
  friend class base::RefCountedThreadSafe<TempScopedDirRegistryCleaner>;
  ~TempScopedDirRegistryCleaner() override;

  std::wstring registry_path_;
};

// Implements HashStoreContents by storing authenticators in the Windows
// registry.
class RegistryHashStoreContentsWin : public HashStoreContents {
 public:
  // Constructs a RegistryHashStoreContents which acts on a registry entry
  // defined by |registry_path| and |store_key|.
  explicit RegistryHashStoreContentsWin(
      const std::wstring& registry_path,
      const std::wstring& store_key,
      scoped_refptr<TempScopedDirCleaner> temp_dir_cleaner);
  ~RegistryHashStoreContentsWin() override;

  // HashStoreContents overrides:
  bool IsCopyable() const override;
  std::unique_ptr<HashStoreContents> MakeCopy() const override;
  std::string_view GetUMASuffix() const override;
  void Reset() override;
  bool GetAtomicPrefAuthenticator(const std::string& path,
                                  std::string* out_value) override;
  bool GetSplitPrefAuthenticators(
      const std::string& path,
      std::map<std::string, std::string>* split_macs) override;
  void SetAtomicPrefAuthenticator(const std::string& path,
                                  const std::string& value) override;
  void SetSplitPrefAuthenticator(const std::string& path,
                                 const std::string& split_path,
                                 const std::string& value) override;
  bool RemoveAuthenticator(const std::string& path) override;
  bool SupportsSuperAuthenticator() const override;

  // Unsupported HashStoreContents overrides:
  void ImportAuthenticator(const std::string& path,
                           const base::Value* in_value) override;
  const base::DictValue* GetContents() const override;
  std::string GetSuperHmac() const override;
  void SetSuperHmac(const std::string& super_hmac) override;
  std::string GetSuperEncryptedHash() const override;
  void SetSuperEncryptedHash(const std::string& super_encrypted_hash) override;

 private:
  // Helper constructor for |MakeCopy|.
  explicit RegistryHashStoreContentsWin(
      const RegistryHashStoreContentsWin& other);

  const std::wstring preference_key_name_;
  scoped_refptr<TempScopedDirCleaner> temp_dir_cleaner_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_
