// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/registry_hash_store_contents_win.h"

#include <windows.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "services/preferences/public/cpp/tracked/tracked_preference_histogram_names.h"

using base::win::RegistryValueIterator;

namespace {

constexpr size_t kMacSize = 64;

std::wstring GetSplitPrefKeyName(const std::wstring& reg_key_name,
                                 const std::string& split_key_name) {
  return reg_key_name + L"\\" + base::UTF8ToWide(split_key_name);
}

bool ReadMacFromRegistry(const base::win::RegKey& key,
                         const std::string& value_name,
                         std::string* out_mac) {
  std::wstring string_value;
  if (key.ReadValue(base::UTF8ToWide(value_name).c_str(), &string_value) ==
          ERROR_SUCCESS &&
      string_value.size() == kMacSize) {
    out_mac->assign(base::WideToUTF8(string_value));
    return true;
  }
  return false;
}

// Removes |value_name| under |reg_key_name|. Returns true if found and
// successfully removed.
bool ClearAtomicMac(const std::wstring& reg_key_name,
                    const std::string& value_name) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, reg_key_name.c_str(),
               KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    return key.DeleteValue(base::UTF8ToWide(value_name).c_str()) ==
           ERROR_SUCCESS;
  }
  return false;
}

// Deletes |split_key_name| under |reg_key_name|. Returns true if found and
// successfully removed.
bool ClearSplitMac(const std::wstring& reg_key_name,
                   const std::string& split_key_name) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER,
               GetSplitPrefKeyName(reg_key_name, split_key_name).c_str(),
               KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    return key.DeleteKey(L"") == ERROR_SUCCESS;
  }
  return false;
}

// Deletes |reg_key_name| if it exists.
void DeleteRegistryKey(const std::wstring& reg_key_name) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, reg_key_name.c_str(),
               KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    LONG result = key.DeleteKey(L"");
    DCHECK(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND) << result;
  }
}

}  // namespace

void TempScopedDirRegistryCleaner::SetRegistryPath(
    const std::wstring& registry_path) {
  if (registry_path_.empty())
    registry_path_ = registry_path;
  else
    DCHECK_EQ(registry_path_, registry_path);
}

TempScopedDirRegistryCleaner::~TempScopedDirRegistryCleaner() {
  DCHECK(!registry_path_.empty());
  DeleteRegistryKey(registry_path_);
}

RegistryHashStoreContentsWin::RegistryHashStoreContentsWin(
    const std::wstring& registry_path,
    const std::wstring& store_key,
    scoped_refptr<TempScopedDirCleaner> temp_dir_cleaner)
    : preference_key_name_(registry_path + L"\\PreferenceMACs\\" + store_key),
      temp_dir_cleaner_(std::move(temp_dir_cleaner)) {
  if (temp_dir_cleaner_)
    static_cast<TempScopedDirRegistryCleaner*>(temp_dir_cleaner_.get())
        ->SetRegistryPath(preference_key_name_);
}

RegistryHashStoreContentsWin::~RegistryHashStoreContentsWin() = default;

RegistryHashStoreContentsWin::RegistryHashStoreContentsWin(
    const RegistryHashStoreContentsWin& other) = default;

bool RegistryHashStoreContentsWin::IsCopyable() const {
  return true;
}

std::unique_ptr<HashStoreContents> RegistryHashStoreContentsWin::MakeCopy()
    const {
  return base::WrapUnique(new RegistryHashStoreContentsWin(*this));
}

std::string_view RegistryHashStoreContentsWin::GetUMASuffix() const {
  return user_prefs::tracked::kTrackedPrefRegistryValidationSuffix;
}

void RegistryHashStoreContentsWin::Reset() {
  DeleteRegistryKey(preference_key_name_);
}

bool RegistryHashStoreContentsWin::GetMac(const std::string& path,
                                          std::string* out_value) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, preference_key_name_.c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    return ReadMacFromRegistry(key, path, out_value);
  }

  return false;
}

bool RegistryHashStoreContentsWin::GetSplitMacs(
    const std::string& path,
    std::map<std::string, std::string>* split_macs) {
  DCHECK(split_macs);
  DCHECK(split_macs->empty());

  RegistryValueIterator iter_key(
      HKEY_CURRENT_USER,
      GetSplitPrefKeyName(preference_key_name_, path).c_str());

  for (; iter_key.Valid(); ++iter_key) {
    split_macs->insert(make_pair(base::WideToUTF8(iter_key.Name()),
                                 base::WideToUTF8(iter_key.Value())));
  }

  return !split_macs->empty();
}

void RegistryHashStoreContentsWin::SetMac(const std::string& path,
                                          const std::string& value) {
  base::win::RegKey key;
  DCHECK_EQ(kMacSize, value.size());

  if (key.Create(HKEY_CURRENT_USER, preference_key_name_.c_str(),
                 KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    key.WriteValue(base::UTF8ToWide(path).c_str(),
                   base::UTF8ToWide(value).c_str());
  }
}

void RegistryHashStoreContentsWin::SetSplitMac(const std::string& path,
                                               const std::string& split_path,
                                               const std::string& value) {
  base::win::RegKey key;
  DCHECK_EQ(kMacSize, value.size());

  if (key.Create(HKEY_CURRENT_USER,
                 GetSplitPrefKeyName(preference_key_name_, path).c_str(),
                 KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    key.WriteValue(base::UTF8ToWide(split_path).c_str(),
                   base::UTF8ToWide(value).c_str());
  }
}

bool RegistryHashStoreContentsWin::RemoveEntry(const std::string& path) {
  return ClearAtomicMac(preference_key_name_, path) ||
         ClearSplitMac(preference_key_name_, path);
}

void RegistryHashStoreContentsWin::ImportEntry(const std::string& path,
                                               const base::Value* in_value) {
  NOTREACHED_IN_MIGRATION()
      << "RegistryHashStoreContents does not support the ImportEntry operation";
}

const base::Value::Dict* RegistryHashStoreContentsWin::GetContents() const {
  NOTREACHED_IN_MIGRATION()
      << "RegistryHashStoreContents does not support the GetContents operation";
  return NULL;
}

std::string RegistryHashStoreContentsWin::GetSuperMac() const {
  NOTREACHED_IN_MIGRATION()
      << "RegistryHashStoreContents does not support the GetSuperMac operation";
  return NULL;
}

void RegistryHashStoreContentsWin::SetSuperMac(const std::string& super_mac) {
  NOTREACHED_IN_MIGRATION()
      << "RegistryHashStoreContents does not support the SetSuperMac operation";
}
