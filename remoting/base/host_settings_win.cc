// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/host_settings_win.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

#if defined(OFFICIAL_BUILD)
static constexpr wchar_t kHostSettingsKeyName[] =
    L"SOFTWARE\\Google\\Chrome Remote Desktop\\HostSettings";
#else
static constexpr wchar_t kHostSettingsKeyName[] =
    L"SOFTWARE\\Chromoting\\HostSettings";
#endif

}  // namespace

HostSettingsWin::HostSettingsWin() = default;
HostSettingsWin::~HostSettingsWin() = default;

void HostSettingsWin::InitializeInstance() {
  LONG result;
  result = settings_root_key_.Open(HKEY_LOCAL_MACHINE, kHostSettingsKeyName,
                                   KEY_READ | KEY_WRITE);
  if (result == ERROR_ACCESS_DENIED) {
    HOST_LOG << "Cannot open registry key with write permission. Trying again "
             << "with readonly permission instead.";
    result = settings_root_key_.Open(HKEY_LOCAL_MACHINE, kHostSettingsKeyName,
                                     KEY_READ);
  }
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to open key HKLM\\" << kHostSettingsKeyName
               << ", result: " << result;
  }
}

std::string HostSettingsWin::GetString(const HostSettingKey key,
                                       const std::string& default_value) const {
  std::wstring value;
  std::wstring wide_key = base::UTF8ToWide(key);
  LONG result = settings_root_key_.ReadValue(wide_key.c_str(), &value);
  if (result == ERROR_FILE_NOT_FOUND) {
    return default_value;
  } else if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to read value for " << key << ", result: " << result;
    return default_value;
  }
  return base::WideToUTF8(value);
}

void HostSettingsWin::SetString(const HostSettingKey key,
                                const std::string& value) {
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
