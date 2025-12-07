// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_H_
#define REMOTING_HOST_HOST_CONFIG_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"

namespace base {
class FilePath;
}  // namespace base

namespace remoting {

// Following constants define names for configuration parameters.

// The email address of the account which owns this remote access host instance.
// The value of `kDeprecatedHostOwnerEmailConfigPath` will be used if both keys
// exist in the config for backward compatibility.
extern const char kHostOwnerConfigPath[];
// Service account used to make web service requests and communicate over the
// signaling channel. Prefer reading this value, over
// `kDeprecatedXmppLoginConfigPath` if both exist.
extern const char kServiceAccountConfigPath[];
// OAuth refresh token which is associated with the host service account. This
// token is exchanged for an access token which is used for web service
// authentication.
extern const char kOAuthRefreshTokenConfigPath[];
// Unique identifier of the host used to register the host in directory.
// Normally a random UUID.
extern const char kHostIdConfigPath[];
// Readable host name.
extern const char kHostNameConfigPath[];
// Hash of the host secret used for authentication.
extern const char kHostSecretHashConfigPath[];
// Private key used for host authentication.
extern const char kPrivateKeyConfigPath[];
// Whether consent is given for usage stats reporting.
extern const char kUsageStatsConsentConfigPath[];
// Indicates whether the machine is configured for session authorization.
extern const char kRequireSessionAuthorizationPath[];
// A hint used when initializing the host before it comes online. Several
// actions, such as validating the host config itself, require knowing the
// context in which the host is being run. An example is whether a PIN secret
// should exist in the config or not. This value should match the scopes stored
// in the refresh token, otherwise the host will appear to come online but will
// not be connectable.
extern const char kHostTypeHintPath[];
// Supported Host type hint values stored in |kHostTypeHintPath|.
extern const char kCorpHostTypeHint[];
extern const char kCloudHostTypeHint[];
extern const char kMe2MeHostTypeHint[];

// Deprecated keys. These keys were used in pre-M120 host versions and are being
// kept around for backward compatibility. We should consider rewriting the
// config file at some point so we no longer need to support them.

// host_owner and host_owner_email were both required when we relied on Google
// Talk for signaling as these fields did not match for some account types.
// Though we no longer rely on that service, existing hosts may still have a
// config which uses this key so we read from it as needed.
extern const char kDeprecatedHostOwnerEmailConfigPath[];
// xmpp_login is a legacy term which was used with Google Talk. Though we no
// longer rely on that service, existing hosts may still have this key in their
// configuration file so we read from it as needed.
// This key was replaced by `kServiceAccountConfigPath` in M120.
extern const char kDeprecatedXmppLoginConfigPath[];

// Helpers for serializing/deserializing Host configuration dictionaries.
std::optional<base::Value::Dict> HostConfigFromJson(
    const std::string& serialized);
std::string HostConfigToJson(const base::Value::Dict& host_config);

// Helpers for loading/saving host configurations from/to files.
std::optional<base::Value::Dict> HostConfigFromJsonFile(
    const base::FilePath& config_file);
bool HostConfigToJsonFile(const base::Value::Dict& host_config,
                          const base::FilePath& config_file);

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
