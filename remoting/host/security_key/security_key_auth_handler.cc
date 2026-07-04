// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <atomic>
#include <memory>

#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/security_key/security_key_auth_handler_mojo.h"

#if BUILDFLAG(IS_POSIX)
#include "remoting/host/security_key/security_key_auth_handler_posix.h"
#endif

namespace remoting {

namespace {

std::atomic<bool> g_use_mojo_handler{false};

SecurityKeyAuthHandler::CreateHandlerCallbackForTesting& GetTestingCallback() {
  static base::NoDestructor<
      SecurityKeyAuthHandler::CreateHandlerCallbackForTesting>
      g_callback;
  return *g_callback;
}

}  // namespace

// static
void SecurityKeyAuthHandler::set_use_mojo_handler(bool use_mojo_handler) {
  g_use_mojo_handler = use_mojo_handler;
}

// static
void SecurityKeyAuthHandler::SetCreateHandlerCallbackForTesting(
    CreateHandlerCallbackForTesting callback) {
  GetTestingCallback() = std::move(callback);
}

// static
std::unique_ptr<SecurityKeyAuthHandler> SecurityKeyAuthHandler::Create(
    ClientSessionDetails* client_session_details) {
  if (!GetTestingCallback().is_null()) {
    return GetTestingCallback().Run(client_session_details);
  }

  std::unique_ptr<SecurityKeyAuthHandler> auth_handler;
  if (g_use_mojo_handler) {
    auth_handler =
        std::make_unique<SecurityKeyAuthHandlerMojo>(client_session_details);
  } else {
#if BUILDFLAG(IS_POSIX)
    auth_handler = std::make_unique<SecurityKeyAuthHandlerPosix>();
#else
    NOTIMPLEMENTED();
#endif
  }
  return auth_handler;
}

void SecurityKeyAuthHandler::BindSecurityKeyForwarder(
    mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
