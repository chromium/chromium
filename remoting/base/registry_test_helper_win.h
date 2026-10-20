// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_REGISTRY_TEST_HELPER_WIN_H_
#define REMOTING_BASE_REGISTRY_TEST_HELPER_WIN_H_

#include <windows.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace remoting {

inline constexpr std::array<REGSAM, 2> kBothRegistryViews = {KEY_WOW64_32KEY,
                                                             KEY_WOW64_64KEY};

// Helper class for querying Windows registry state and security descriptors in
// unit tests. Overloads that omit `root_key` default to `HKEY_LOCAL_MACHINE`.
class RegistryTestHelperWin {
 public:
  RegistryTestHelperWin() = delete;

  // Returns true if `key_path` exists under `root_key` in the specified
  // registry view (`wow64_access`).
  static bool KeyExists(HKEY root_key,
                        const std::wstring& key_path,
                        REGSAM wow64_access = 0);
  static bool KeyExists(const std::wstring& key_path, REGSAM wow64_access = 0);

  // Reads a string value (`REG_SZ`) from `key_path` under `root_key`. Pass
  // `value_name = nullptr` to read the default (unnamed) value. Returns
  // `std::nullopt` if the key or value does not exist or cannot be read.
  static std::optional<std::wstring> ReadStringValue(
      HKEY root_key,
      const std::wstring& key_path,
      const wchar_t* value_name = nullptr,
      REGSAM wow64_access = 0);
  static std::optional<std::wstring> ReadStringValue(
      const std::wstring& key_path,
      const wchar_t* value_name = nullptr,
      REGSAM wow64_access = 0);

  // Reads a `DWORD` value (`REG_DWORD`) from `key_path` under `root_key`.
  // Returns `std::nullopt` if the key or value does not exist.
  static std::optional<DWORD> ReadDwordValue(HKEY root_key,
                                             const std::wstring& key_path,
                                             const wchar_t* value_name,
                                             REGSAM wow64_access = 0);
  static std::optional<DWORD> ReadDwordValue(const std::wstring& key_path,
                                             const wchar_t* value_name,
                                             REGSAM wow64_access = 0);

  // Reads a raw binary value (`REG_BINARY`) from `key_path` under `root_key`.
  // Returns `std::nullopt` if the key or value does not exist or is not of type
  // `REG_BINARY`.
  static std::optional<std::vector<uint8_t>> ReadBinaryValue(
      HKEY root_key,
      const std::wstring& key_path,
      const wchar_t* value_name,
      REGSAM wow64_access = 0);
  static std::optional<std::vector<uint8_t>> ReadBinaryValue(
      const std::wstring& key_path,
      const wchar_t* value_name,
      REGSAM wow64_access = 0);

  // Converts a binary `SECURITY_DESCRIPTOR` into an SDDL string (including
  // owner, group, DACL, and mandatory label). Returns `std::nullopt` if `sd` is
  // null, invalid, or cannot be converted.
  static std::optional<std::wstring> SdToSddl(PSECURITY_DESCRIPTOR sd);

  // Reads a `REG_BINARY` security descriptor value from `key_path` and converts
  // it to an SDDL string via `SdToSddl()`.
  static std::optional<std::wstring> ReadBinarySdAsSddl(
      HKEY root_key,
      const std::wstring& key_path,
      const wchar_t* value_name,
      REGSAM wow64_access = 0);
  static std::optional<std::wstring> ReadBinarySdAsSddl(
      const std::wstring& key_path,
      const wchar_t* value_name,
      REGSAM wow64_access = 0);
};

}  // namespace remoting

#endif  // REMOTING_BASE_REGISTRY_TEST_HELPER_WIN_H_
