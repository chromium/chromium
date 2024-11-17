// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/values.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"

namespace remoting {

bool GetUsageStatsConsent(bool& allowed, bool& set_by_policy) {
  set_by_policy = false;
  allowed = false;

  // Normally, the ConfigFileWatcher class would be used for retrieving config
  // settings, but this code needs to execute before Breakpad is initialized,
  // which itself should happen as early as possible during startup.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHostConfigSwitchName)) {
    base::FilePath config_file_path =
        command_line->GetSwitchValuePath(kHostConfigSwitchName);
    std::optional<base::Value::Dict> host_config(
        HostConfigFromJsonFile(config_file_path));
    if (host_config.has_value()) {
      std::optional<bool> host_config_value =
          host_config->FindBool(kUsageStatsConsentConfigPath);
      if (host_config_value.has_value()) {
        allowed = *host_config_value;
        return true;
      }
    }
  }
  return false;
}

bool IsUsageStatsAllowed() {
  bool allowed;
  bool set_by_policy;
  return GetUsageStatsConsent(allowed, set_by_policy) && allowed;
}

bool SetUsageStatsConsent(bool allowed) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace remoting
