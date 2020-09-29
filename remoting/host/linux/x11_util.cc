// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_util.h"

#include "base/bind.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xtest.h"

namespace remoting {

static ScopedXErrorHandler* g_handler = nullptr;

ScopedXErrorHandler::ScopedXErrorHandler(const Handler& handler)
    : handler_(handler), ok_(true) {
  // This is a non-exhaustive check for incorrect usage. It doesn't handle the
  // case where a mix of ScopedXErrorHandler and raw XSetErrorHandler calls are
  // used, and it disallows nested ScopedXErrorHandlers on the same thread,
  // despite these being perfectly safe.
  DCHECK(g_handler == nullptr);
  g_handler = this;
  previous_handler_ = XSetErrorHandler(HandleXErrors);
}

ScopedXErrorHandler::~ScopedXErrorHandler() {
  g_handler = nullptr;
  XSetErrorHandler(previous_handler_);
}

int ScopedXErrorHandler::HandleXErrors(Display* display, XErrorEvent* error) {
  DCHECK(g_handler != nullptr);
  g_handler->ok_ = false;
  if (g_handler->handler_)
    g_handler->handler_.Run(display, error);
  return 0;
}

ScopedXGrabServer::ScopedXGrabServer(x11::Connection* connection)
    : connection_(connection) {
  connection_->GrabServer({});
}

ScopedXGrabServer::~ScopedXGrabServer() {
  connection_->UngrabServer({});
  connection_->Flush();
}

bool IgnoreXServerGrabs(x11::Connection* connection, bool ignore) {
  if (!connection->xtest()
           .GetVersion({x11::Test::major_version, x11::Test::minor_version})
           .Sync()) {
    return false;
  }

  connection->xtest().GrabControl({ignore});
  return true;
}

}  // namespace remoting
