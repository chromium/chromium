// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client.h"

#include <windows.h>

#include <cstdint>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/typed_buffer.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/win/rdp_client_window.h"

namespace remoting {

namespace {

// 127.0.0.1 is explicitly blocked by the RDP ActiveX control, so we use
// 127.0.0.2 instead.
const unsigned char kRdpLoopbackAddress[] = { 127, 0, 0, 2 };

}  // namespace

// The core of RdpClient is ref-counted since it services calls and notifies
// events on the caller task runner, but runs the ActiveX control on the UI
// task runner.
class RdpClient::Core
    : public base::RefCountedThreadSafe<Core>,
      public RdpClientWindow::EventHandler {
 public:
  Core(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      RdpClient::EventHandler* event_handler);

  // Initiates a loopback RDP connection.
  void Connect(const ScreenResolution& resolution,
               const std::string& terminal_id,
               DWORD port_number);

  // Initiates a graceful shutdown of the RDP connection.
  void Disconnect();

  // Sends Secure Attention Sequence to the session.
  void InjectSas();

  // Change the resolution of the desktop.
  void ChangeResolution(const ScreenResolution& resolution);

  // RdpClientWindow::EventHandler interface.
  void OnConnected() override;
  void OnDisconnected() override;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() override;

  // Helpers for the event handler's methods that make sure that OnRdpClosed()
  // is the last notification delivered and is delevered only once.
  void NotifyConnected();
  void NotifyClosed();

  // Task runner on which the caller expects |event_handler_| to be notified.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |rdp_client_window_| is running.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Event handler receiving notification about connection state. The pointer is
  // cleared when Disconnect() methods is called, stopping any further updates.
  RdpClient::EventHandler* event_handler_;

  // Hosts the RDP ActiveX control.
  std::unique_ptr<RdpClientWindow> rdp_client_window_;

  // A self-reference to keep the object alive during connection shutdown.
  scoped_refptr<Core> self_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

RdpClient::RdpClient(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const ScreenResolution& resolution,
    const std::string& terminal_id,
    DWORD port_number,
    EventHandler* event_handler) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());

  core_ = new Core(caller_task_runner, ui_task_runner, event_handler);
  core_->Connect(resolution, terminal_id, port_number);
}

RdpClient::~RdpClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_->Disconnect();
}

void RdpClient::InjectSas() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_->InjectSas();
}

void RdpClient::ChangeResolution(const ScreenResolution& resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_->ChangeResolution(resolution);
}

RdpClient::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    RdpClient::EventHandler* event_handler)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      event_handler_(event_handler) {
}

void RdpClient::Core::Connect(const ScreenResolution& resolution,
                              const std::string& terminal_id,
                              DWORD port_number) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::Connect, this, resolution, terminal_id,
                                  port_number));
    return;
  }

  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DCHECK(!rdp_client_window_);
  DCHECK(!self_.get());

  net::IPEndPoint server_endpoint(net::IPAddress(kRdpLoopbackAddress),
                                  base::checked_cast<uint16_t>(port_number));

  // Create the ActiveX control window.
  rdp_client_window_.reset(new RdpClientWindow(server_endpoint, terminal_id,
                                               this));
  if (!rdp_client_window_->Connect(resolution)) {
    rdp_client_window_.reset();

    // Notify the caller that connection attempt failed.
    NotifyClosed();
  }
}

void RdpClient::Core::Disconnect() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&Core::Disconnect, this));
    return;
  }

  // The caller does not expect any notifications to be delivered after this
  // point.
  event_handler_ = nullptr;

  // Gracefully shutdown the RDP connection.
  if (rdp_client_window_) {
    self_ = this;
    rdp_client_window_->Disconnect();
  }
}

void RdpClient::Core::InjectSas() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&Core::InjectSas, this));
    return;
  }

  if (rdp_client_window_) {
    rdp_client_window_->InjectSas();
  }
}

void RdpClient::Core::ChangeResolution(const ScreenResolution& resolution) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::ChangeResolution, this, resolution));
    return;
  }

  if (rdp_client_window_) {
    rdp_client_window_->ChangeResolution(resolution);
  }
}

void RdpClient::Core::OnConnected() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(rdp_client_window_);

  NotifyConnected();
}

void RdpClient::Core::OnDisconnected() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(rdp_client_window_);

  NotifyClosed();

  // Delay window destruction until no ActiveX control's code is on the stack.
  ui_task_runner_->DeleteSoon(FROM_HERE, rdp_client_window_.release());
  self_ = nullptr;
}

RdpClient::Core::~Core() {
  DCHECK(!event_handler_);
  DCHECK(!rdp_client_window_);
}

void RdpClient::Core::NotifyConnected() {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&Core::NotifyConnected, this));
    return;
  }

  if (event_handler_)
    event_handler_->OnRdpConnected();
}

void RdpClient::Core::NotifyClosed() {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&Core::NotifyClosed, this));
    return;
  }

  if (event_handler_) {
    RdpClient::EventHandler* event_handler = event_handler_;
    event_handler_ = nullptr;
    event_handler->OnRdpClosed();
  }
}

}  // namespace remoting
