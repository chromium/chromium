// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_H_
#define REMOTING_HOST_HOST_CONFIG_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace remoting {

// Following constants define names for configuration parameters.

// Status of the host, whether it is enabled or disabled.
extern const char kHostEnabledConfigPath[];
// Base JID of the host owner.
// TODO(yuweih): This is now always equal to the host owner email. Consider
// unifying these two fields.
extern const char kHostOwnerConfigPath[];
// Email of the owner of this host.
extern const char kHostOwnerEmailConfigPath[];
// Login used to authenticate signaling.
extern const char kXmppLoginConfigPath[];
// OAuth refresh token used to fetch an access token for calling network APIs.
extern const char kOAuthRefreshTokenConfigPath[];
// Unique identifier of the host used to register the host in directory.
// Normally a random UUID.
extern const char kHostIdConfigPath[];
// Readable host name.
extern const char kHostNameConfigPath[];
// Hash of the host secret used for authentication.
extern const char kHostSecretHashConfigPath[];
// Private keys used for host authentication.
extern const char kPrivateKeyConfigPath[];
// Whether consent is given for usage stats reporting.
extern const char kUsageStatsConsentConfigPath[];
// Whether to offer VP9 encoding to clients.
extern const char kEnableVp9ConfigPath[];
// Whether to offer H264 encoding to clients.
extern const char kEnableH264ConfigPath[];
// Number of Kibibytes of frame data to allow each client to record.
extern const char kFrameRecorderBufferKbConfigPath[];

// Helpers for serializing/deserializing Host configuration dictionaries.
absl::optional<base::Value::Dict> HostConfigFromJson(
    const std::string& serialized);
std::string HostConfigToJson(const base::Value::Dict& host_config);

// Helpers for loading/saving host configurations from/to files.
absl::optional<base::Value::Dict> HostConfigFromJsonFile(
    const base::FilePath& config_file);
bool HostConfigToJsonFile(const base::Value::Dict& host_config,
                          const base::FilePath& config_file);

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
