// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_HOST_SETTINGS_WIN_H_
#define REMOTING_BASE_HOST_SETTINGS_WIN_H_

#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "remoting/base/host_settings.h"

namespace remoting {

// HostSettings implementation for Windows that stores settings in the HKLM hive
// in the registry.
class HostSettingsWin final : public HostSettings {
 public:
  HostSettingsWin();
  HostSettingsWin(const HostSettingsWin&) = delete;
  HostSettingsWin& operator=(const HostSettingsWin&) = delete;
  ~HostSettingsWin() override;

  // HostSettings implementation.
  void InitializeInstance() override;
  std::string GetString(const HostSettingKey key,
                        const std::string& default_value) const override;
  void SetString(const HostSettingKey key, const std::string& value) override;

 private:
  base::win::RegKey settings_root_key_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_HOST_SETTINGS_WIN_H_
