// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace remoting {

// Current values.
const char kHostOwnerConfigPath[] = "host_owner";
const char kServiceAccountConfigPath[] = "service_account";
const char kOAuthRefreshTokenConfigPath[] = "oauth_refresh_token";
const char kHostIdConfigPath[] = "host_id";
const char kHostNameConfigPath[] = "host_name";
const char kHostSecretHashConfigPath[] = "host_secret_hash";
const char kPrivateKeyConfigPath[] = "private_key";
const char kUsageStatsConsentConfigPath[] = "usage_stats_consent";
const char kRequireSessionAuthorizationPath[] = "require_session_authz";
const char kHostTypeHintPath[] = "host_type_hint";
const char kCorpHostTypeHint[] = "corp";
const char kCloudHostTypeHint[] = "cloud";
const char kMe2MeHostTypeHint[] = "me2me";

// Deprecated values.
const char kDeprecatedHostOwnerEmailConfigPath[] = "host_owner_email";
const char kDeprecatedXmppLoginConfigPath[] = "xmpp_login";

std::optional<base::Value::Dict> HostConfigFromJson(const std::string& json) {
  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value.has_value()) {
    LOG(ERROR) << "Failed to parse host config from JSON";
    return std::nullopt;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Parsed host config returned was not a dictionary";
    return std::nullopt;
  }
  auto config = std::move(value->GetDict());

  // The service_account field was added in M120 so this key will not be present
  // if the host was configured using an earlier package version. For that case,
  // we read from xmpp_login and use that value if it is present. Otherwise the
  // config is considered to be malformed.
  if (!config.FindString(kServiceAccountConfigPath)) {
    auto xmpp_login = config.Extract(kDeprecatedXmppLoginConfigPath);
    if (xmpp_login.has_value()) {
      config.Set(kServiceAccountConfigPath, xmpp_login->GetString());
    } else {
      LOG(WARNING) << "Host config is missing values for both: "
                   << kServiceAccountConfigPath << " and "
                   << kDeprecatedXmppLoginConfigPath;
    }
  }

  // Legacy configs may have both host_owner and host_owner_email due to the way
  // we integrated with Google Talk. If host_owner_email exists, we should use
  // its value rather than use host_owner which is likely a Google Talk JID.
  auto host_owner_email = config.Extract(kDeprecatedHostOwnerEmailConfigPath);
  if (host_owner_email.has_value()) {
    LOG(INFO) << "Replacing the value of `" << kHostOwnerConfigPath << "` with "
              << *host_owner_email;
    config.Set(kHostOwnerConfigPath, host_owner_email->GetString());
  }

  return std::move(config);
}

std::string HostConfigToJson(const base::Value::Dict& host_config) {
  std::string data;
  base::JSONWriter::Write(host_config, &data);
  return data;
}

std::optional<base::Value::Dict> HostConfigFromJsonFile(
    const base::FilePath& config_file) {
  std::string serialized;
  if (!base::ReadFileToString(config_file, &serialized)) {
    LOG(ERROR) << "Failed to read " << config_file.value();
    return std::nullopt;
  }

  return HostConfigFromJson(serialized);
}

bool HostConfigToJsonFile(const base::Value::Dict& host_config,
                          const base::FilePath& config_file) {
  std::string serialized = HostConfigToJson(host_config);
  return base::ImportantFileWriter::WriteFileAtomically(config_file,
                                                        serialized);
}

}  // namespace remoting
