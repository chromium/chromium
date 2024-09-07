// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/is_google_email.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"

namespace remoting {

bool GetUsageStatsConsent(bool* allowed, bool* set_by_policy) {
  *set_by_policy = false;
  *allowed = false;

  std::string filename = GetHostHash() + ".json";
  base::FilePath config_path = GetConfigDirectoryPath().Append(filename);
  std::optional<base::Value::Dict> config(HostConfigFromJsonFile(config_path));
  if (!config.has_value()) {
    LOG(ERROR) << "No host config file found.";
    return false;
  }

  bool initialized = false;
  auto usage_stats_consent = config->FindBool(kUsageStatsConsentConfigPath);
  if (usage_stats_consent.has_value()) {
    initialized = true;
    *allowed = *usage_stats_consent;
  }
  const std::string* host_owner_ptr = config->FindString(kHostOwnerConfigPath);
  if (host_owner_ptr) {
    // Override crash reporting for Google hosts.
    initialized = true;
    *allowed |= IsGoogleEmail(*host_owner_ptr);
  }

  // Indicate whether |allowed| was successfully initialized.
  return initialized;
}

bool IsUsageStatsAllowed() {
  bool allowed;
  bool set_by_policy;
  return GetUsageStatsConsent(&allowed, &set_by_policy) && allowed;
}

bool SetUsageStatsConsent(bool allowed) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace remoting
