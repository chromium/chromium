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
// Despite the pathname, all authenticators for individual tracked prefs are
// stored under this path. The pathname was chosen when the only authenticators
// used were MACs, but it's hard to change.
const char kPrefAuthenticatorsPath[] = "protection.macs";
const char kSuperHmacPref[] = "protection.super_mac";
const char kSuperEncryptedHashPref[] = "protection.super_encrypted_hash";
}

DictionaryHashStoreContents::DictionaryHashStoreContents(
    base::DictValue& storage)
    : storage_(storage) {}

bool DictionaryHashStoreContents::IsCopyable() const {
  return false;
}

std::unique_ptr<HashStoreContents> DictionaryHashStoreContents::MakeCopy()
    const {
  NOTREACHED() << "DictionaryHashStoreContents does not support MakeCopy";
}

std::string_view DictionaryHashStoreContents::GetUMASuffix() const {
  // To stay consistent with existing reported data, do not append a suffix
  // when reporting UMA stats for this content.
  return std::string_view();
}

void DictionaryHashStoreContents::Reset() {
  storage_->RemoveByDottedPath(kPrefAuthenticatorsPath);
}

bool DictionaryHashStoreContents::GetAtomicPrefAuthenticator(
    const std::string& path,
    std::string* out_value) {
  const base::DictValue* authenticators_dict = GetContents();
  if (!authenticators_dict) {
    return false;
  }

  const std::string* str = authenticators_dict->FindStringByDottedPath(path);
  if (!str)
    return false;

  if (out_value)
    *out_value = *str;

  return true;
}

bool DictionaryHashStoreContents::GetSplitPrefAuthenticators(
    const std::string& path,
    std::map<std::string, std::string>* split_pref_authenticators) {
  DCHECK(split_pref_authenticators);
  DCHECK(split_pref_authenticators->empty());

  const base::DictValue* authenticators_dict = GetContents();
  if (!authenticators_dict) {
    return false;
  }
  const base::DictValue* split_authenticators_dict =
      authenticators_dict->FindDictByDottedPath(path);
  if (!split_authenticators_dict) {
    return false;
  }
  for (const auto item : *split_authenticators_dict) {
    const std::string* mac_string = item.second.GetIfString();
    if (!mac_string) {
      NOTREACHED();
    }
    split_pref_authenticators->insert(make_pair(item.first, *mac_string));
  }
  return true;
}

void DictionaryHashStoreContents::SetAtomicPrefAuthenticator(
    const std::string& path,
    const std::string& value) {
  base::DictValue* authenticators_dict = GetMutableContents(true);
  authenticators_dict->SetByDottedPath(path, value);
}

void DictionaryHashStoreContents::SetSplitPrefAuthenticator(
    const std::string& path,
    const std::string& split_path,
    const std::string& value) {
  base::DictValue* authenticators_dict = GetMutableContents(true);
  base::DictValue* split_dict = authenticators_dict->FindDictByDottedPath(path);
  if (!split_dict) {
    split_dict = &authenticators_dict->SetByDottedPath(path, base::DictValue())
                      ->GetDict();
  }
  split_dict->Set(split_path, value);
}

void DictionaryHashStoreContents::ImportAuthenticator(
    const std::string& path,
    const base::Value* in_value) {
  base::DictValue* authenticators_dict = GetMutableContents(true);
  authenticators_dict->SetByDottedPath(path, in_value->Clone());
}

bool DictionaryHashStoreContents::RemoveAuthenticator(const std::string& path) {
  base::DictValue* authenticators_dict = GetMutableContents(false);
  if (authenticators_dict) {
    return authenticators_dict->RemoveByDottedPath(path);
  }

  return false;
}

bool DictionaryHashStoreContents::SupportsSuperAuthenticator() const {
  return true;
}

std::string DictionaryHashStoreContents::GetSuperHmac() const {
  if (const std::string* super_hmac_string =
          storage_->FindStringByDottedPath(kSuperHmacPref)) {
    return *super_hmac_string;
  }
  return std::string();
}

void DictionaryHashStoreContents::SetSuperHmac(const std::string& super_hmac) {
  storage_->SetByDottedPath(kSuperHmacPref, super_hmac);
}

std::string DictionaryHashStoreContents::GetSuperEncryptedHash() const {
  if (const std::string* super_encrypted_hash_string =
          storage_->FindStringByDottedPath(kSuperEncryptedHashPref)) {
    return *super_encrypted_hash_string;
  }
  return std::string();
}

void DictionaryHashStoreContents::SetSuperEncryptedHash(
    const std::string& super_encrypted_hash) {
  storage_->SetByDottedPath(kSuperEncryptedHashPref, super_encrypted_hash);
}

const base::DictValue* DictionaryHashStoreContents::GetContents() const {
  return storage_->FindDictByDottedPath(kPrefAuthenticatorsPath);
}

base::DictValue* DictionaryHashStoreContents::GetMutableContents(
    bool create_if_null) {
  base::DictValue* authenticators_dict =
      storage_->FindDictByDottedPath(kPrefAuthenticatorsPath);
  if (!authenticators_dict && create_if_null) {
    authenticators_dict =
        &storage_->SetByDottedPath(kPrefAuthenticatorsPath, base::DictValue())
             ->GetDict();
  }
  return authenticators_dict;
}
