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
inline constexpr char kHostOwnerConfigPath[] = "host_owner";
// Service account used to make web service requests and communicate over the
// signaling channel. Prefer reading this value, over
// `kDeprecatedXmppLoginConfigPath` if both exist.
inline constexpr char kServiceAccountConfigPath[] = "service_account";
// OAuth refresh token which is associated with the host service account. This
// token is exchanged for an access token which is used for web service
// authentication.
inline constexpr char kOAuthRefreshTokenConfigPath[] = "oauth_refresh_token";
// Unique identifier of the host used to register the host in directory.
// Normally a random UUID.
inline constexpr char kHostIdConfigPath[] = "host_id";
// Readable host name.
inline constexpr char kHostNameConfigPath[] = "host_name";
// Hash of the host secret used for authentication.
inline constexpr char kHostSecretHashConfigPath[] = "host_secret_hash";
// Private key used for host authentication.
inline constexpr char kPrivateKeyConfigPath[] = "private_key";
// Whether consent is given for usage stats reporting.
inline constexpr char kUsageStatsConsentConfigPath[] = "usage_stats_consent";
// Indicates whether the machine is configured for session authorization.
inline constexpr char kRequireSessionAuthorizationPath[] =
    "require_session_authz";
// A hint used when initializing the host before it comes online. Several
// actions, such as validating the host config itself, require knowing the
// context in which the host is being run. An example is whether a PIN secret
// should exist in the config or not. This value should match the scopes stored
// in the refresh token, otherwise the host will appear to come online but will
// not be connectable.
inline constexpr char kHostTypeHintPath[] = "host_type_hint";
// Supported Host type hint values stored in |kHostTypeHintPath|.
inline constexpr char kCorpHostTypeHint[] = "corp";
inline constexpr char kCloudHostTypeHint[] = "cloud";
inline constexpr char kMe2MeHostTypeHint[] = "me2me";

// Deprecated keys. These keys were used in pre-M120 host versions and are being
// kept around for backward compatibility. We should consider rewriting the
// config file at some point so we no longer need to support them.

// host_owner and host_owner_email were both required when we relied on Google
// Talk for signaling as these fields did not match for some account types.
// Though we no longer rely on that service, existing hosts may still have a
// config which uses this key so we read from it as needed.
inline constexpr char kDeprecatedHostOwnerEmailConfigPath[] =
    "host_owner_email";
// xmpp_login is a legacy term which was used with Google Talk. Though we no
// longer rely on that service, existing hosts may still have this key in their
// configuration file so we read from it as needed.
// This key was replaced by `kServiceAccountConfigPath` in M120.
inline constexpr char kDeprecatedXmppLoginConfigPath[] = "xmpp_login";

// Helpers for serializing/deserializing Host configuration dictionaries.
std::optional<base::DictValue> HostConfigFromJson(
    const std::string& serialized);
std::string HostConfigToJson(const base::DictValue& host_config);

// Helpers for loading/saving host configurations from/to files.
std::optional<base::DictValue> HostConfigFromJsonFile(
    const base::FilePath& config_file);
bool HostConfigToJsonFile(const base::DictValue& host_config,
                          const base::FilePath& config_file);

// Returns true if the host configuration indicates a corporate host instance.
bool IsCorpHostConfig(const base::DictValue& host_config);

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_H_
