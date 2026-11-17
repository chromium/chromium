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
    // Rejects all incoming connections (e.g. when config is `{}` on a non-corp
    // host).
    kRejectAll,
    // Routes all connections to `session_user()`.
    kSessionUser,
    // Routes connections to the local user matching the client's corporate
    // username. Used on corp hosts when `sessionUser` is not set.
    kMatchCorpClientUsername,
  };

  // Returns the default path:
  // `/etc/chrome-remote-desktop/session_routing.json`.
  static base::FilePath GetDefaultConfigFilePath();

  // Synchronously loads and validates the config file from `path`.
  // - Returns `base::ok(std::nullopt)` if the file or its parent directory
  //   does not exist.
  // - Returns `base::ok(SessionRoutingConfig)` if the file is secure and valid.
  // - Returns `base::unexpected(Loggable)` if the file or its parent directory
  //   fails the ownership/permission/symlink security checks of
  //   ReadSecureFile(), or if the file contains malformed JSON or violates the
  //   schema.
  // `is_corp_host` selects the routing mode when `sessionUser` is not set:
  // `kMatchCorpClientUsername` on corp hosts, `kRejectAll` otherwise.
  static base::expected<std::optional<SessionRoutingConfig>, Loggable>
  LoadAndValidate(const base::FilePath& path = GetDefaultConfigFilePath(),
                  bool is_corp_host = false,
                  uid_t expected_owner_uid = 0);

  // Returns whether `username` is accepted as a local Linux username, e.g. for
  // `sessionUser`. Only ASCII letters, digits, '_', '.' and '-' are allowed,
  // and the name must not start with '-', so it is also safe to use in systemd
  // unit names and command lines. Note that names used in unit names are
  // additionally bounded by IsUserNameSafeForUnitName() in host_types.h (e.g.
  // they are at most 200 characters long).
  static bool IsValidUsername(std::string_view username);

  SessionRoutingConfig();
  // Derives `mode()` from `session_user` and `is_corp_host`.
  SessionRoutingConfig(std::string session_user,
                       bool create_remote_user_sessions,
                       bool is_corp_host = false);
  ~SessionRoutingConfig();

  SessionRoutingConfig(const SessionRoutingConfig&);
  SessionRoutingConfig& operator=(const SessionRoutingConfig&);
  SessionRoutingConfig(SessionRoutingConfig&&);
  SessionRoutingConfig& operator=(SessionRoutingConfig&&);

  bool operator==(const SessionRoutingConfig&) const = default;

  // Atomically writes this session routing config to `path` with mode 0644,
  // owned by the effective user (root in production). Performs blocking file
  // I/O.
  //
  // `session_user()` must be empty or a valid username, and the parent
  // directory of `path` must already exist, be owned by the effective user,
  // and not be writable by group or others.
  //
  // An empty `session_user()` omits the `sessionUser` field, and
  // `create_remote_user_sessions()` is only written if it is true, so a
  // default-constructed config writes `{}`.
  base::expected<void, Loggable> WriteToFile(
      const base::FilePath& path = GetDefaultConfigFilePath()) const;

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

  // Returns whether the host should create a new remote session for the
  // resolved user, if supported, when the user has no graphical session.
  // Created sessions are not terminated when the client disconnects, and are
  // reused by the next connection, including across host upgrades and daemon
  // crashes. They are terminated when the host service is stopped. Defaults to
  // `false`.
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
