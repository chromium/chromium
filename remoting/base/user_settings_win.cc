// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/user_settings_win.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_types.h"

namespace remoting {

namespace {

#if defined(OFFICIAL_BUILD)
static constexpr wchar_t kUserSettingsKeyName[] =
    L"SOFTWARE\\Google\\Chrome Remote Desktop\\UserSettings";
#else
static constexpr wchar_t kUserSettingsKeyName[] =
    L"SOFTWARE\\Chromoting\\UserSettings";
#endif

}  // namespace

UserSettingsWin::UserSettingsWin() {
  LONG result;
  result = settings_root_key_.Create(HKEY_CURRENT_USER, kUserSettingsKeyName,
                                     KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    LOG(DFATAL) << "Failed to create/open key HKCU\\" << kUserSettingsKeyName
                << ", result: " << result;
  }
}

UserSettingsWin::~UserSettingsWin() = default;

std::string UserSettingsWin::GetString(const UserSettingKey key) const {
  if (!settings_root_key_.Valid()) {
    LOG(ERROR) << "Can't get value for " << key
               << " since registry key is invalid.";
    return std::string();
  }

  std::wstring wide_key = base::UTF8ToWide(key);
  std::wstring wide_value;
  LONG result = settings_root_key_.ReadValue(wide_key.c_str(), &wide_value);
  if (result == ERROR_FILE_NOT_FOUND) {
    // No setting for the given key.
    return std::string();
  }
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to read value for " << key << ", result: " << result;
    return std::string();
  }
  return base::WideToUTF8(wide_value);
}

void UserSettingsWin::SetString(const UserSettingKey key,
                                const std::string& value) {
  if (!settings_root_key_.Valid()) {
    LOG(ERROR) << "Can't set value for " << key
               << " since registry key is invalid.";
    return;
  }

  std::wstring wide_key = base::UTF8ToWide(key);
  std::wstring wide_value = base::UTF8ToWide(value);

  LONG result =
      settings_root_key_.WriteValue(wide_key.c_str(), wide_value.c_str());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write value " << value << " to key " << key
               << ", result: " << result;
  }
}

}  // namespace remoting
