// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/dictionary_hash_store_contents.h"

#include <ostream>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/values.h"

namespace {
const char kPreferenceMACs[] = "protection.macs";
const char kSuperMACPref[] = "protection.super_mac";
}

DictionaryHashStoreContents::DictionaryHashStoreContents(
    base::Value::Dict& storage)
    : storage_(storage) {}

bool DictionaryHashStoreContents::IsCopyable() const {
  return false;
}

std::unique_ptr<HashStoreContents> DictionaryHashStoreContents::MakeCopy()
    const {
  NOTREACHED_IN_MIGRATION()
      << "DictionaryHashStoreContents does not support MakeCopy";
  return nullptr;
}

std::string_view DictionaryHashStoreContents::GetUMASuffix() const {
  // To stay consistent with existing reported data, do not append a suffix
  // when reporting UMA stats for this content.
  return std::string_view();
}

void DictionaryHashStoreContents::Reset() {
  storage_->RemoveByDottedPath(kPreferenceMACs);
}

bool DictionaryHashStoreContents::GetMac(const std::string& path,
                                         std::string* out_value) {
  const base::Value::Dict* macs_dict = GetContents();
  if (!macs_dict)
    return false;

  const std::string* str = macs_dict->FindStringByDottedPath(path);
  if (!str)
    return false;

  if (out_value)
    *out_value = *str;

  return true;
}

bool DictionaryHashStoreContents::GetSplitMacs(
    const std::string& path,
    std::map<std::string, std::string>* split_macs) {
  DCHECK(split_macs);
  DCHECK(split_macs->empty());

  const base::Value::Dict* macs_dict = GetContents();
  if (!macs_dict)
    return false;
  const base::Value::Dict* split_macs_dict =
      macs_dict->FindDictByDottedPath(path);
  if (!split_macs_dict)
    return false;
  for (const auto item : *split_macs_dict) {
    const std::string* mac_string = item.second.GetIfString();
    if (!mac_string) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    split_macs->insert(make_pair(item.first, *mac_string));
  }
  return true;
}

void DictionaryHashStoreContents::SetMac(const std::string& path,
                                         const std::string& value) {
  base::Value::Dict* macs_dict = GetMutableContents(true);
  macs_dict->SetByDottedPath(path, value);
}

void DictionaryHashStoreContents::SetSplitMac(const std::string& path,
                                              const std::string& split_path,
                                              const std::string& value) {
  base::Value::Dict* macs_dict = GetMutableContents(true);
  base::Value::Dict* split_dict = macs_dict->FindDictByDottedPath(path);
  if (!split_dict) {
    split_dict =
        &macs_dict->SetByDottedPath(path, base::Value::Dict())->GetDict();
  }
  split_dict->Set(split_path, value);
}

void DictionaryHashStoreContents::ImportEntry(const std::string& path,
                                              const base::Value* in_value) {
  base::Value::Dict* macs_dict = GetMutableContents(true);
  macs_dict->SetByDottedPath(path, in_value->Clone());
}

bool DictionaryHashStoreContents::RemoveEntry(const std::string& path) {
  base::Value::Dict* macs_dict = GetMutableContents(false);
  if (macs_dict)
    return macs_dict->RemoveByDottedPath(path);

  return false;
}

std::string DictionaryHashStoreContents::GetSuperMac() const {
  if (const std::string* super_mac_string =
          storage_->FindStringByDottedPath(kSuperMACPref)) {
    return *super_mac_string;
  }
  return std::string();
}

void DictionaryHashStoreContents::SetSuperMac(const std::string& super_mac) {
  storage_->SetByDottedPath(kSuperMACPref, super_mac);
}

const base::Value::Dict* DictionaryHashStoreContents::GetContents() const {
  return storage_->FindDictByDottedPath(kPreferenceMACs);
}

base::Value::Dict* DictionaryHashStoreContents::GetMutableContents(
    bool create_if_null) {
  base::Value::Dict* macs_dict =
      storage_->FindDictByDottedPath(kPreferenceMACs);
  if (!macs_dict && create_if_null) {
    macs_dict = &storage_->SetByDottedPath(kPreferenceMACs, base::Value::Dict())
                     ->GetDict();
  }
  return macs_dict;
}
