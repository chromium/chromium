// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_LOGIN_SESSION_REPORTER_SERVER_H_
#define REMOTING_HOST_LINUX_LOGIN_SESSION_REPORTER_SERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "remoting/host/mojom/login_session.mojom.h"

namespace remoting {

// Class to run a mojo server on a named platform channel for the session
// reporter process to report information about a systemd login session. See
// remoting/host/mojom/login_session.mojom for more info.
class LoginSessionReporterServer : public mojom::LoginSessionObserver {
 public:
  explicit LoginSessionReporterServer(
      mojom::LoginSessionObserver* session_reporter);
  ~LoginSessionReporterServer() override;

  LoginSessionReporterServer(const LoginSessionReporterServer&) = delete;
  LoginSessionReporterServer& operator=(const LoginSessionReporterServer&) =
      delete;

  void StartServer();
  void StopServer();

 private:
  // mojom::LoginSessionObserver implementation.
  void OnLoginSessionCreated(mojom::LoginSessionInfoPtr session_info) override;

  raw_ptr<mojom::LoginSessionObserver> session_reporter_;
  named_mojo_ipc_server::NamedMojoIpcServer<mojom::LoginSessionObserver>
      ipc_server_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_LOGIN_SESSION_REPORTER_SERVER_H_
