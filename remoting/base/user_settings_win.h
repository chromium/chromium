// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_USER_SETTINGS_WIN_H_
#define REMOTING_BASE_USER_SETTINGS_WIN_H_

#include <string>

#include "base/win/registry.h"
#include "remoting/base/user_settings.h"

namespace remoting {

// UserSettings implementation for Windows that stores settings in Windows
// registry. The settings will be stored in HKCU.
class UserSettingsWin final : public UserSettings {
 public:
  UserSettingsWin();
  ~UserSettingsWin() override;

  std::string GetString(const UserSettingKey key) const override;
  void SetString(const UserSettingKey key, const std::string& value) override;

  UserSettingsWin(const UserSettingsWin&) = delete;
  UserSettingsWin& operator=(const UserSettingsWin&) = delete;

 private:
  base::win::RegKey settings_root_key_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_USER_SETTINGS_WIN_H_
