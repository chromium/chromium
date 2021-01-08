// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SETTINGS_H_
#define REMOTING_HOST_HOST_SETTINGS_H_

#include <string>

#include "base/values.h"

namespace remoting {

using HostSettingKey = char[];

// A class to read host settings, which are simple key-value pairs unrelated to
// the host's identity, like UID of an audio device to capture audio from, or
// whether a feature should be enabled.
class HostSettings {
 public:
  // Initializes host settings. Must be called on a thread that allows blocking
  // before calling GetValue().
  static void Initialize();

  static HostSettings* GetInstance();

  // Gets the value of the setting. Returns empty string if the value is not
  // found.
  virtual std::string GetString(const HostSettingKey key) const = 0;

  HostSettings(const HostSettings&) = delete;
  HostSettings& operator=(const HostSettings&) = delete;

 protected:
  HostSettings();
  virtual ~HostSettings();

  virtual void InitializeInstance() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SETTINGS_H_
