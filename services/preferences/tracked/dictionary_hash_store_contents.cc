// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/dictionary_hash_store_contents.h"

#include "base/callback.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/persistent_pref_store.h"

namespace {
const char kPreferenceMACs[] = "protection.macs";
const char kSuperMACPref[] = "protection.super_mac";
}

DictionaryHashStoreContents::DictionaryHashStoreContents(
    base::DictionaryValue* storage)
    : storage_(storage) {}

// static
void DictionaryHashStoreContents::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kPreferenceMACs);
  registry->RegisterStringPref(kSuperMACPref, std::string());
}

bool DictionaryHashStoreContents::IsCopyable() const {
  return false;
}

std::unique_ptr<HashStoreContents> DictionaryHashStoreContents::MakeCopy()
    const {
  NOTREACHED() << "DictionaryHashStoreContents does not support MakeCopy";
  return nullptr;
}

base::StringPiece DictionaryHashStoreContents::GetUMASuffix() const {
  // To stay consistent with existing reported data, do not append a suffix
  // when reporting UMA stats for this content.
  return base::StringPiece();
}

void DictionaryHashStoreContents::Reset() {
  storage_->RemovePath(kPreferenceMACs);
}

bool DictionaryHashStoreContents::GetMac(const std::string& path,
                                         std::string* out_value) {
  const base::DictionaryValue* macs_dict = GetContents();
  if (macs_dict)
    return macs_dict->GetString(path, out_value);

  return false;
}

bool DictionaryHashStoreContents::GetSplitMacs(
    const std::string& path,
    std::map<std::string, std::string>* split_macs) {
  DCHECK(split_macs);
  DCHECK(split_macs->empty());

  const base::DictionaryValue* macs_dict = GetContents();
  const base::DictionaryValue* split_macs_dict = NULL;
  if (!macs_dict || !macs_dict->GetDictionary(path, &split_macs_dict))
    return false;
  for (base::DictionaryValue::Iterator it(*split_macs_dict); !it.IsAtEnd();
       it.Advance()) {
    const std::string* mac_string = it.value().GetIfString();
    if (!mac_string) {
      NOTREACHED();
      continue;
    }
    split_macs->insert(make_pair(it.key(), *mac_string));
  }
  return true;
}

void DictionaryHashStoreContents::SetMac(const std::string& path,
                                         const std::string& value) {
  base::DictionaryValue* macs_dict = GetMutableContents(true);
  macs_dict->SetString(path, value);
}

void DictionaryHashStoreContents::SetSplitMac(const std::string& path,
                                              const std::string& split_path,
                                              const std::string& value) {
  base::DictionaryValue* macs_dict = GetMutableContents(true);
  base::Value* split_dict = macs_dict->FindDictPath(path);
  if (!split_dict) {
    split_dict =
        macs_dict->SetPath(path, base::Value(base::Value::Type::DICTIONARY));
  }
  split_dict->SetKey(split_path, base::Value(value));
}

void DictionaryHashStoreContents::ImportEntry(const std::string& path,
                                              const base::Value* in_value) {
  base::DictionaryValue* macs_dict = GetMutableContents(true);
  macs_dict->SetPath(path, in_value->Clone());
}

bool DictionaryHashStoreContents::RemoveEntry(const std::string& path) {
  base::DictionaryValue* macs_dict = GetMutableContents(false);
  if (macs_dict)
    return macs_dict->RemovePath(path);

  return false;
}

std::string DictionaryHashStoreContents::GetSuperMac() const {
  std::string super_mac_string;
  storage_->GetString(kSuperMACPref, &super_mac_string);
  return super_mac_string;
}

void DictionaryHashStoreContents::SetSuperMac(const std::string& super_mac) {
  storage_->SetString(kSuperMACPref, super_mac);
}

const base::DictionaryValue* DictionaryHashStoreContents::GetContents() const {
  const base::DictionaryValue* macs_dict = NULL;
  storage_->GetDictionary(kPreferenceMACs, &macs_dict);
  return macs_dict;
}

base::DictionaryValue* DictionaryHashStoreContents::GetMutableContents(
    bool create_if_null) {
  base::DictionaryValue* macs_dict = NULL;
  storage_->GetDictionary(kPreferenceMACs, &macs_dict);
  if (!macs_dict && create_if_null) {
    macs_dict = static_cast<base::DictionaryValue*>(storage_->SetPath(
        kPreferenceMACs, base::Value(base::Value::Type::DICTIONARY)));
  }
  return macs_dict;
}
