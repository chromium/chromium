// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_LOGIN_SESSION_MANAGER_H_
#define REMOTING_HOST_LINUX_LOGIN_SESSION_MANAGER_H_

#include <sys/types.h>

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// Class to manage systemd login sessions. This is basically a wrapper on
// org.freedesktop.login1 interfaces. See:
// https://man7.org/linux/man-pages/man5/org.freedesktop.login1.5.html
//
// This class requires current process to be run as root.
class LoginSessionManager {
 public:
  // A struct with information about a session object.
  struct SessionInfo {
    SessionInfo();
    SessionInfo(SessionInfo&&);
    SessionInfo(const SessionInfo&);
    SessionInfo& operator=(SessionInfo&&);
    SessionInfo& operator=(const SessionInfo&);
    ~SessionInfo();

    // The D-Bus object path of the session.
    gvariant::ObjectPath object_path;

    // The session ID.
    std::string session_id;

    // The session class. Typical values are "user", "greeter", "lock-screen".
    std::string session_class;

    // The session type. Typical values are "x11", "wayland", "tty",
    // "unspecified".
    std::string session_type;

    // The username of the user.
    std::string username;

    // The UID of the user.
    uid_t uid;

    // Whether the session is remote.
    bool is_remote;
  };

  using GetSessionInfoCallback =
      base::OnceCallback<void(base::expected<SessionInfo, Loggable>)>;
  using TerminateSessionCallback =
      base::OnceCallback<void(base::expected<void, Loggable>)>;

  // `connection`: An initiated system bus connection.
  explicit LoginSessionManager(GDBusConnectionRef connection);
  ~LoginSessionManager();

  LoginSessionManager(const LoginSessionManager&) = delete;
  LoginSessionManager& operator=(const LoginSessionManager&) = delete;

  void GetSessionInfo(const std::string& session_id,
                      GetSessionInfoCallback callback);
  void TerminateSession(const gvariant::ObjectPath& session_object_path,
                        TerminateSessionCallback callback);

 private:
  void OnGetSessionPathResult(
      GetSessionInfoCallback callback,
      base::expected<std::tuple<gvariant::ObjectPath>, Loggable> result);
  void OnGetSessionPropertiesResult(
      gvariant::ObjectPath session_path,
      GetSessionInfoCallback callback,
      base::expected<std::tuple<gvariant::GVariantRef<"a{sv}">>, Loggable>
          result);
  void OnTerminateSessionResult(TerminateSessionCallback callback,
                                base::expected<std::tuple<>, Loggable> result);

  SEQUENCE_CHECKER(sequence_checker_);

  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<LoginSessionManager> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_LOGIN_SESSION_MANAGER_H_
