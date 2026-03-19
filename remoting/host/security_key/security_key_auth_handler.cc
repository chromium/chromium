// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/security_key/security_key_auth_handler_mojo.h"

#if BUILDFLAG(IS_POSIX)
#include "remoting/host/security_key/security_key_auth_handler_posix.h"
#endif

namespace remoting {

namespace {

bool g_use_mojo_handler = false;

}  // namespace

// static
void SecurityKeyAuthHandler::set_use_mojo_handler(bool use_mojo_handler) {
  g_use_mojo_handler = use_mojo_handler;
}

// static
std::unique_ptr<SecurityKeyAuthHandler> SecurityKeyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler;
  if (g_use_mojo_handler) {
    auth_handler =
        std::make_unique<SecurityKeyAuthHandlerMojo>(client_session_details);
  } else {
#if BUILDFLAG(IS_POSIX)
    auth_handler =
        std::make_unique<SecurityKeyAuthHandlerPosix>(file_task_runner);
#else
    NOTIMPLEMENTED();
#endif
  }

  if (auth_handler) {
    auth_handler->SetSendMessageCallback(send_message_callback);
  }
  return auth_handler;
}

void SecurityKeyAuthHandler::BindSecurityKeyForwarder(
    mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
