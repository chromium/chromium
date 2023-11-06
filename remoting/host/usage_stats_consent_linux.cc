// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <memory>
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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

bool GetUsageStatsConsent(bool* allowed, bool* set_by_policy) {
  *set_by_policy = false;
  *allowed = false;

  std::string filename = GetHostHash() + ".json";
  base::FilePath config_path = GetConfigDirectoryPath().Append(filename);
  absl::optional<base::Value::Dict> config(HostConfigFromJsonFile(config_path));
  if (!config.has_value()) {
    LOG(ERROR) << "No host config file found.";
    return false;
  }

  // TODO(joedow): Check kUsageStatsConsentConfigPath to enable crash reporting
  // for non-Google Linux hosts.  Also requires modifying the native message
  // host to write the value during setup.
  const std::string* host_owner_ptr = config->FindString(kHostOwnerConfigPath);
  if (!host_owner_ptr) {
    LOG(ERROR) << "Host config has no host_owner field.";
    return false;
  }

  *allowed = IsGoogleEmail(*host_owner_ptr);

  // Indicate that |allowed| was successfully initialized.
  return true;
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
