// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/registry_test_helper_win.h"

#include <windows.h>

#include <sddl.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/win/registry.h"
#include "base/win/scoped_localalloc.h"

namespace remoting {

// static
bool RegistryTestHelperWin::KeyExists(HKEY root_key,
                                      const std::wstring& key_path,
                                      REGSAM wow64_access) {
  return base::win::RegKey(root_key, key_path.c_str(), KEY_READ | wow64_access)
      .Valid();
}

// static
bool RegistryTestHelperWin::KeyExists(const std::wstring& key_path,
                                      REGSAM wow64_access) {
  return KeyExists(HKEY_LOCAL_MACHINE, key_path, wow64_access);
}

// static
std::optional<std::wstring> RegistryTestHelperWin::ReadStringValue(
    HKEY root_key,
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  base::win::RegKey key(root_key, key_path.c_str(), KEY_READ | wow64_access);
  if (!key.Valid()) {
    return std::nullopt;
  }
  std::wstring value;
  if (key.ReadValue(value_name, &value) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  return value;
}

// static
std::optional<std::wstring> RegistryTestHelperWin::ReadStringValue(
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  return ReadStringValue(HKEY_LOCAL_MACHINE, key_path, value_name,
                         wow64_access);
}

// static
std::optional<DWORD> RegistryTestHelperWin::ReadDwordValue(
    HKEY root_key,
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  base::win::RegKey key(root_key, key_path.c_str(), KEY_READ | wow64_access);
  if (!key.Valid()) {
    return std::nullopt;
  }
  DWORD value = 0;
  if (key.ReadValueDW(value_name, &value) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  return value;
}

// static
std::optional<DWORD> RegistryTestHelperWin::ReadDwordValue(
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  return ReadDwordValue(HKEY_LOCAL_MACHINE, key_path, value_name, wow64_access);
}

// static
std::optional<std::vector<uint8_t>> RegistryTestHelperWin::ReadBinaryValue(
    HKEY root_key,
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  base::win::RegKey key(root_key, key_path.c_str(), KEY_READ | wow64_access);
  if (!key.Valid()) {
    return std::nullopt;
  }
  DWORD size = 0;
  DWORD type = 0;
  if (key.ReadValue(value_name, nullptr, &size, &type) != ERROR_SUCCESS ||
      type != REG_BINARY) {
    return std::nullopt;
  }
  std::vector<uint8_t> buffer(size);
  if (key.ReadValue(value_name, buffer.data(), &size, &type) != ERROR_SUCCESS) {
    return std::nullopt;
  }
  buffer.resize(size);
  return buffer;
}

// static
std::optional<std::vector<uint8_t>> RegistryTestHelperWin::ReadBinaryValue(
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  return ReadBinaryValue(HKEY_LOCAL_MACHINE, key_path, value_name,
                         wow64_access);
}

// static
std::optional<std::wstring> RegistryTestHelperWin::SdToSddl(
    PSECURITY_DESCRIPTOR sd) {
  if (!sd || !::IsValidSecurityDescriptor(sd)) {
    return std::nullopt;
  }
  LPWSTR raw_sddl = nullptr;
  constexpr SECURITY_INFORMATION kAllSdInfo =
      OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
      DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION;
  if (!::ConvertSecurityDescriptorToStringSecurityDescriptorW(
          sd, SDDL_REVISION_1, kAllSdInfo, &raw_sddl, nullptr)) {
    return std::nullopt;
  }
  base::win::ScopedLocalAllocTyped<wchar_t> scoped_sddl(raw_sddl);
  return std::wstring(scoped_sddl.get());
}

// static
std::optional<std::wstring> RegistryTestHelperWin::ReadBinarySdAsSddl(
    HKEY root_key,
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  std::optional<std::vector<uint8_t>> bytes =
      ReadBinaryValue(root_key, key_path, value_name, wow64_access);
  if (!bytes.has_value() || bytes->empty()) {
    return std::nullopt;
  }
  return SdToSddl(bytes->data());
}

// static
std::optional<std::wstring> RegistryTestHelperWin::ReadBinarySdAsSddl(
    const std::wstring& key_path,
    const wchar_t* value_name,
    REGSAM wow64_access) {
  return ReadBinarySdAsSddl(HKEY_LOCAL_MACHINE, key_path, value_name,
                            wow64_access);
}

}  // namespace remoting
