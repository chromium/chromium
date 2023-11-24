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

// Implements HashStoreContents by storing MACs in the Windows registry.
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
  bool GetMac(const std::string& path, std::string* out_value) override;
  bool GetSplitMacs(const std::string& path,
                    std::map<std::string, std::string>* split_macs) override;
  void SetMac(const std::string& path, const std::string& value) override;
  void SetSplitMac(const std::string& path,
                   const std::string& split_path,
                   const std::string& value) override;
  bool RemoveEntry(const std::string& path) override;

  // Unsupported HashStoreContents overrides:
  void ImportEntry(const std::string& path,
                   const base::Value* in_value) override;
  const base::Value::Dict* GetContents() const override;
  std::string GetSuperMac() const override;
  void SetSuperMac(const std::string& super_mac) override;

 private:
  // Helper constructor for |MakeCopy|.
  explicit RegistryHashStoreContentsWin(
      const RegistryHashStoreContentsWin& other);

  const std::wstring preference_key_name_;
  scoped_refptr<TempScopedDirCleaner> temp_dir_cleaner_;
};

#endif  // SERVICES_PREFERENCES_TRACKED_REGISTRY_HASH_STORE_CONTENTS_WIN_H_
