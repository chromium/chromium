// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/security_key_ipc_server_impl.h"

namespace {

// Not thread safe, tests which set this value must do so on the same thread.
static remoting::SecurityKeyIpcServerFactory* g_factory = nullptr;

}  // namespace

namespace remoting {

void SecurityKeyIpcServer::SetFactoryForTest(
    SecurityKeyIpcServerFactory* factory) {
  g_factory = factory;
}

std::unique_ptr<SecurityKeyIpcServer> SecurityKeyIpcServer::Create(
    int connection_id,
    ClientSessionDetails* client_session_details,
    base::TimeDelta initial_connect_timeout,
    const SecurityKeyAuthHandler::SendMessageCallback& message_callback,
    base::OnceClosure connect_callback,
    base::OnceClosure done_callback) {
  std::unique_ptr<SecurityKeyIpcServer> ipc_server =
      g_factory ? g_factory->Create(connection_id, client_session_details,
                                    initial_connect_timeout, message_callback,
                                    std::move(connect_callback),
                                    std::move(done_callback))
                : base::WrapUnique(new SecurityKeyIpcServerImpl(
                      connection_id, client_session_details,
                      initial_connect_timeout, message_callback,
                      std::move(connect_callback), std::move(done_callback)));

  return ipc_server;
}

}  // namespace remoting
