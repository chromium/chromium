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

const char kHostEnabledConfigPath[] = "enabled";
const char kHostOwnerConfigPath[] = "host_owner";
const char kHostOwnerEmailConfigPath[] = "host_owner_email";
const char kXmppLoginConfigPath[] = "xmpp_login";
const char kOAuthRefreshTokenConfigPath[] = "oauth_refresh_token";
const char kHostIdConfigPath[] = "host_id";
const char kHostNameConfigPath[] = "host_name";
const char kHostSecretHashConfigPath[] = "host_secret_hash";
const char kPrivateKeyConfigPath[] = "private_key";
const char kUsageStatsConsentConfigPath[] = "usage_stats_consent";
const char kEnableVp9ConfigPath[] = "enable_vp9";
const char kEnableH264ConfigPath[] = "enable_h264";
const char kFrameRecorderBufferKbConfigPath[] = "frame-recorder-buffer-kb";

absl::optional<base::Value::Dict> HostConfigFromJson(const std::string& json) {
  absl::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value.has_value()) {
    DLOG(ERROR) << "Failed to parse host config from JSON";
    return absl::nullopt;
  }

  if (!value->is_dict()) {
    DLOG(ERROR) << "Parsed host config returned was not a dictionary";
    return absl::nullopt;
  }

  return std::move(value->GetDict());
}

std::string HostConfigToJson(const base::Value::Dict& host_config) {
  std::string data;
  base::JSONWriter::Write(host_config, &data);
  return data;
}

absl::optional<base::Value::Dict> HostConfigFromJsonFile(
    const base::FilePath& config_file) {
  std::string serialized;
  if (!base::ReadFileToString(config_file, &serialized)) {
    DLOG(ERROR) << "Failed to read " << config_file.value();
    return absl::nullopt;
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
