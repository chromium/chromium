// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_config.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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
const char kIsFtlTokenConfigPath[] = "is_ftl_token";

std::unique_ptr<base::DictionaryValue> HostConfigFromJson(
    const std::string& json) {
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value || !value->is_dict()) {
    LOG(WARNING) << "Failed to parse host config from JSON";
    return nullptr;
  }

  return base::WrapUnique(static_cast<base::DictionaryValue*>(value.release()));
}

std::string HostConfigToJson(const base::DictionaryValue& host_config) {
  std::string data;
  base::JSONWriter::Write(host_config, &data);
  return data;
}

std::unique_ptr<base::DictionaryValue> HostConfigFromJsonFile(
    const base::FilePath& config_file) {
  // TODO(sergeyu): Implement better error handling here.
  std::string serialized;
  if (!base::ReadFileToString(config_file, &serialized)) {
    LOG(WARNING) << "Failed to read " << config_file.value();
    return nullptr;
  }

  return HostConfigFromJson(serialized);
}

bool HostConfigToJsonFile(const base::DictionaryValue& host_config,
                          const base::FilePath& config_file) {
  std::string serialized = HostConfigToJson(host_config);
  return base::ImportantFileWriter::WriteFileAtomically(config_file,
                                                        serialized);
}

}  // namespace remoting
