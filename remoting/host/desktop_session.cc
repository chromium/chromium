// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session.h"

#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/daemon_process.h"

namespace remoting {

DesktopSession::~DesktopSession() = default;

DesktopSession::DesktopSession(DaemonProcess* daemon_process, int id)
    : daemon_process_(daemon_process), id_(id) {}

}  // namespace remoting
