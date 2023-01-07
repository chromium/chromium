// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_url_forwarder_configurator.h"

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcUrlForwarderConfigurator::IpcUrlForwarderConfigurator(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {}

IpcUrlForwarderConfigurator::~IpcUrlForwarderConfigurator() = default;

void IpcUrlForwarderConfigurator::IsUrlForwarderSetUp(
    IsUrlForwarderSetUpCallback callback) {
  desktop_session_proxy_->IsUrlForwarderSetUp(std::move(callback));
}

void IpcUrlForwarderConfigurator::SetUpUrlForwarder(
    const SetUpUrlForwarderCallback& callback) {
  desktop_session_proxy_->SetUpUrlForwarder(callback);
}

}  // namespace remoting
