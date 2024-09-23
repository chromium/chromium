// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_DICTIONARY_HASH_STORE_CONTENTS_H_
#define SERVICES_PREFERENCES_TRACKED_DICTIONARY_HASH_STORE_CONTENTS_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "services/preferences/tracked/hash_store_contents.h"

// Implements HashStoreContents by storing MACs in a DictionaryValue. The
// DictionaryValue is presumed to be the contents of a PrefStore.
// RegisterProfilePrefs() may be used to register all of the preferences used by
// this object.
class DictionaryHashStoreContents : public HashStoreContents {
 public:
  // Constructs a DictionaryHashStoreContents that reads from and writes to
  // |storage|.
  explicit DictionaryHashStoreContents(base::Value::Dict& storage);

  DictionaryHashStoreContents(const DictionaryHashStoreContents&) = delete;
  DictionaryHashStoreContents& operator=(const DictionaryHashStoreContents&) =
      delete;

  // HashStoreContents implementation
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
  void ImportEntry(const std::string& path,
                   const base::Value* in_value) override;
  bool RemoveEntry(const std::string& path) override;
  const base::Value::Dict* GetContents() const override;
  std::string GetSuperMac() const override;
  void SetSuperMac(const std::string& super_mac) override;

 private:
  const raw_ref<base::Value::Dict> storage_;

  // Helper function to get a mutable version of the macs from |storage_|,
  // creating it if needed and |create_if_null| is true.
  base::Value::Dict* GetMutableContents(bool create_if_null);
};

#endif  // SERVICES_PREFERENCES_TRACKED_DICTIONARY_HASH_STORE_CONTENTS_H_
