// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/functional/bind.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"

namespace remoting {

namespace {

class DisconnectWindowAura : public HostWindow {
 public:
  DisconnectWindowAura();

  DisconnectWindowAura(const DisconnectWindowAura&) = delete;
  DisconnectWindowAura& operator=(const DisconnectWindowAura&) = delete;

  ~DisconnectWindowAura() override;

  // HostWindow interface.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;
};

DisconnectWindowAura::DisconnectWindowAura() = default;

DisconnectWindowAura::~DisconnectWindowAura() {
  ash::Shell::Get()->system_tray_notifier()->NotifyRemotingScreenShareStop();
}

void DisconnectWindowAura::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  ash::Shell::Get()->system_tray_notifier()->NotifyRemotingScreenShareStart(
      base::BindRepeating(&ClientSessionControl::DisconnectSession,
                          client_session_control, ErrorCode::OK));
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return std::make_unique<DisconnectWindowAura>();
}

}  // namespace remoting
