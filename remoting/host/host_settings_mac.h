// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SETTINGS_MAC_H_
#define REMOTING_HOST_HOST_SETTINGS_MAC_H_

#include <memory>

#include "remoting/host/host_settings.h"

namespace remoting {

class HostSettingsMac final : public HostSettings {
 public:
  HostSettingsMac();
  ~HostSettingsMac() override;

  // HostSettings implementation.
  void InitializeInstance() override;
  std::string GetString(const HostSettingKey key) const override;

  HostSettingsMac(const HostSettingsMac&) = delete;
  HostSettingsMac& operator=(const HostSettingsMac&) = delete;

 private:
  // TODO(yuweih): This needs to be guarded with a lock if we detect changes of
  // the settings file.
  std::unique_ptr<base::Value> settings_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SETTINGS_MAC_H_
