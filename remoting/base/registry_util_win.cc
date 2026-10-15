// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/registry_util_win.h"

#include <windows.h>

#include <array>

#include "base/logging.h"
#include "base/win/registry.h"

namespace remoting {

namespace {

constexpr REGSAM kBothViews = KEY_WOW64_32KEY | KEY_WOW64_64KEY;
constexpr std::array<REGSAM, 2> kRegistryViews = {KEY_WOW64_32KEY,
                                                  KEY_WOW64_64KEY};

}  // namespace

bool WriteDefaultStringValue(HKEY root_key,
                             const std::wstring& key_path,
                             const std::wstring& value,
                             REGSAM wow64_access) {
  if ((wow64_access & kBothViews) == kBothViews) {
    bool success = true;
    for (REGSAM view : kRegistryViews) {
      if (!WriteDefaultStringValue(root_key, key_path, value,
                                   (wow64_access & ~kBothViews) | view)) {
        success = false;
      }
    }
    return success;
  }

  base::win::RegKey key;
  LONG result =
      key.Create(root_key, key_path.c_str(), KEY_SET_VALUE | wow64_access);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create or open key " << key_path << ": " << result;
    return false;
  }
  result = key.WriteValue(nullptr, value.c_str());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write default value for " << key_path << ": "
               << result;
    return false;
  }
  return true;
}

void DeleteKey(HKEY root_key,
               const std::wstring& key_path,
               REGSAM wow64_access) {
  if ((wow64_access & kBothViews) == kBothViews) {
    for (REGSAM view : kRegistryViews) {
      DeleteKey(root_key, key_path, (wow64_access & ~kBothViews) | view);
    }
    return;
  }

  base::win::RegKey root;
  if (root.Open(root_key, L"", KEY_QUERY_VALUE | wow64_access) ==
      ERROR_SUCCESS) {
    LONG result = root.DeleteKey(key_path.c_str());
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << "Failed to delete key " << key_path << ": " << result;
    }
  }
}

bool DeleteKeyIfEmpty(HKEY root_key,
                      const std::wstring& key_path,
                      REGSAM wow64_access) {
  if ((wow64_access & kBothViews) == kBothViews) {
    bool success = true;
    for (REGSAM view : kRegistryViews) {
      if (!DeleteKeyIfEmpty(root_key, key_path,
                            (wow64_access & ~kBothViews) | view)) {
        success = false;
      }
    }
    return success;
  }

  base::win::RegKey key;
  LONG open_res = key.Open(root_key, key_path.c_str(), KEY_READ | wow64_access);
  if (open_res != ERROR_SUCCESS) {
    return open_res == ERROR_FILE_NOT_FOUND;
  }

  DWORD subkey_count = 0;
  DWORD value_count = 0;
  LONG query_res = ::RegQueryInfoKeyW(
      key.Handle(), nullptr, nullptr, nullptr, &subkey_count, nullptr, nullptr,
      &value_count, nullptr, nullptr, nullptr, nullptr);
  key.Close();

  if (query_res != ERROR_SUCCESS) {
    return false;
  }

  if (subkey_count == 0 && value_count == 0) {
    base::win::RegKey root;
    if (root.Open(root_key, L"", KEY_QUERY_VALUE | wow64_access) !=
        ERROR_SUCCESS) {
      return false;
    }
    LONG delete_res = root.DeleteKey(key_path.c_str(),
                                     base::win::RegKey::RecursiveDelete(false));
    if (delete_res != ERROR_SUCCESS && delete_res != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << "Failed to delete empty key " << key_path << ": "
                 << delete_res;
      return false;
    }
    return true;
  }

  return true;
}

}  // namespace remoting
