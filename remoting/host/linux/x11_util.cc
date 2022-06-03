// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_util.h"

#include "base/bind.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xtest.h"

namespace remoting {

ScopedXGrabServer::ScopedXGrabServer(x11::Connection* connection)
    : connection_(connection) {
  connection_->GrabServer();
}

ScopedXGrabServer::~ScopedXGrabServer() {
  connection_->UngrabServer();
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
