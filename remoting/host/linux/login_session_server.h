// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_LOGIN_SESSION_SERVER_H_
#define REMOTING_HOST_LINUX_LOGIN_SESSION_SERVER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"
#include "remoting/host/mojom/login_session.mojom.h"

namespace remoting {

// Class to run a mojo server on a named platform channel for the session
// info process to query information about its session.
class LoginSessionServer : public mojom::LoginSessionService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns whether the given `session_id` is a session that is managed by
    // Chrome Remote Desktop.
    virtual bool IsRunningInCrdSession(const std::string& session_id) = 0;
  };

  explicit LoginSessionServer(Delegate* delegate);
  ~LoginSessionServer() override;

  LoginSessionServer(const LoginSessionServer&) = delete;
  LoginSessionServer& operator=(const LoginSessionServer&) = delete;

  void StartServer();
  void StopServer();

 private:
  // mojom::LoginSessionService implementation.
  void IsRunningInCrdSession(IsRunningInCrdSessionCallback callback) override;

  raw_ptr<Delegate> delegate_;
  named_mojo_ipc_server::NamedMojoIpcServer<mojom::LoginSessionService>
      ipc_server_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_LOGIN_SESSION_SERVER_H_
