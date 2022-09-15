// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_action_executor.h"

#include <utility>

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcActionExecutor::IpcActionExecutor(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {}

IpcActionExecutor::~IpcActionExecutor() = default;

void IpcActionExecutor::ExecuteAction(const protocol::ActionRequest& request) {
  desktop_session_proxy_->ExecuteAction(request);
}

}  // namespace remoting
