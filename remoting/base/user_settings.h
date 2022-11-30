// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_USER_SETTINGS_H_
#define REMOTING_BASE_USER_SETTINGS_H_

#include <string>

namespace remoting {

using UserSettingKey = char[];

// A class to read and write settings for the current user.
class UserSettings {
 public:
  static UserSettings* GetInstance();

  UserSettings(const UserSettings&) = delete;
  UserSettings& operator=(const UserSettings&) = delete;

  // Gets the value of the setting. Returns empty string if the value is not
  // found.
  virtual std::string GetString(const UserSettingKey key) const = 0;

  // Sets a string value for |key| and writes it into the settings file.
  virtual void SetString(const UserSettingKey key,
                         const std::string& value) = 0;

 protected:
  UserSettings();
  virtual ~UserSettings();
};

}  // namespace remoting

#endif  // REMOTING_BASE_USER_SETTINGS_H_
