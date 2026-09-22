// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SESSION_ROUTING_CONFIG_H_
#define REMOTING_HOST_LINUX_SESSION_ROUTING_CONFIG_H_

#include <sys/types.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "remoting/base/errors.h"
#include "remoting/base/loggable.h"

namespace remoting {

// Loads and evaluates the `/etc/chrome-remote-desktop/session_routing.json`
// configuration file used to route incoming CRD connections to local Linux
// user sessions.
class SessionRoutingConfig {
 public:
  enum class RoutingMode {
    // Rejects all incoming connections (e.g. when config is `{}`).
    kRejectAll,
    // Routes all connections to `session_user()`.
    kSessionUser,
    // Routes connections to the local user matching the client's corporate
    // username.
    kMatchCorpClientUsername,
  };

  // Returns the default path:
  // `/etc/chrome-remote-desktop/session_routing.json`.
  static base::FilePath GetDefaultConfigFilePath();

  // Synchronously loads and validates the config file from `path`.
  // - Returns `base::ok(std::nullopt)` if the file does not exist (`ENOENT`).
  // - Returns `base::ok(SessionRoutingConfig)` if the file is secure and valid.
  // - Returns `base::unexpected(Loggable)` if the file exists on disk but fails
  //   ownership/permission/symlink security checks, contains malformed JSON, or
  //   violates the schema.
  static base::expected<std::optional<SessionRoutingConfig>, Loggable>
  LoadAndValidate(const base::FilePath& path = GetDefaultConfigFilePath(),
                  bool is_corp_host = false,
                  uid_t expected_owner_uid = 0);

  SessionRoutingConfig();
  SessionRoutingConfig(RoutingMode mode,
                       std::string session_user,
                       bool create_remote_user_sessions,
                       bool is_corp_host = false);
  ~SessionRoutingConfig();

  SessionRoutingConfig(const SessionRoutingConfig&);
  SessionRoutingConfig& operator=(const SessionRoutingConfig&);
  SessionRoutingConfig(SessionRoutingConfig&&);
  SessionRoutingConfig& operator=(SessionRoutingConfig&&);

  // Resolves the local Linux username for `client_id` (e.g. `user@google.com`
  // or `user@silo.com`).
  // Returns `ErrorCode::SESSION_REJECTED` if `client_id` is not permitted by
  // the active routing policy.
  base::expected<std::string, ErrorCode> ResolveSessionUser(
      std::string_view client_id) const;

  // Returns the active routing mode configured by `session_routing.json`.
  RoutingMode mode() const { return mode_; }

  // Returns the pinned local Linux username when `mode()` is
  // `RoutingMode::kSessionUser`, or an empty string otherwise.
  const std::string& session_user() const { return session_user_; }

  // Returns whether the host should create new headless remote user sessions
  // (e.g. via GDM `CreateUserDisplay`) if an existing graphical session is not
  // found for the resolved user. Defaults to `false`.
  bool create_remote_user_sessions() const {
    return create_remote_user_sessions_;
  }

  // Returns whether the host is configured as a corporate host.
  bool is_corp_host() const { return is_corp_host_; }

 private:
  RoutingMode mode_ = RoutingMode::kRejectAll;
  std::string session_user_;
  bool create_remote_user_sessions_ = false;
  bool is_corp_host_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SESSION_ROUTING_CONFIG_H_
