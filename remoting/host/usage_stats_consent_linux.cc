// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/usage_stats_consent.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "remoting/base/branding.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/is_google_email.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"

namespace remoting {

namespace {

inline base::FilePath GetMultiProcessHostUsageStatsConsentFile() {
  return GetMultiProcessHostGlobalConfigDir().Append(
      kDefaultUnprivilegedConfigFileName);
}

bool GetUsageStatsConsentFromFile(const base::FilePath& path,
                                  bool* allowed,
                                  bool* set_by_policy) {
  *set_by_policy = false;
  *allowed = false;

  JSONFileValueDeserializer deserializer(path);
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (!value) {
    LOG(ERROR) << "Failed to read usage stats consent from file: "
               << error_message << " (error code: " << error_code << ")";
    return false;
  }
  if (!value->is_dict()) {
    LOG(ERROR) << "Usage stats consent file does not contain a dictionary.";
    return false;
  }
  const base::DictValue& dict = value->GetDict();
  auto usage_stats_consent = dict.FindBool(kUsageStatsConsentConfigPath);
  if (usage_stats_consent.has_value()) {
    *allowed = *usage_stats_consent;
    return true;
  }
  // Note: kHostOwnerConfigPath only exists in the per-user host config file.
  const std::string* host_owner_ptr = dict.FindString(kHostOwnerConfigPath);
  if (host_owner_ptr) {
    // Opt into crash reporting for Googlers if not set in the config.
    *allowed = IsGoogleEmail(*host_owner_ptr);
    return true;
  }
  return false;
}

}  // namespace

bool GetUsageStatsConsent(bool* allowed, bool* set_by_policy) {
  std::string consent_content;

  // We don't know if the host is single-process or multi-process, so we always
  // check the consent file first.
  if (base::PathExists(GetMultiProcessHostUsageStatsConsentFile())) {
    return GetUsageStatsConsentFromFile(
        GetMultiProcessHostUsageStatsConsentFile(), allowed, set_by_policy);
  }

  std::string filename = GetHostHash() + ".json";
  base::FilePath config_path = GetConfigDir().Append(filename);
  if (!base::PathExists(config_path)) {
    LOG(ERROR) << "Cannot find usage stats consent file: " << config_path;
    return false;
  }
  return GetUsageStatsConsentFromFile(config_path, allowed, set_by_policy);
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
