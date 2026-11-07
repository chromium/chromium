// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/typed_buffer.h"
#include "remoting/base/util.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/win/rdp_client_window.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

// 127.0.0.1 is explicitly blocked by the RDP ActiveX control, so we use
// 127.0.0.2 instead.
const unsigned char kRdpLoopbackAddress[] = {127, 0, 0, 2};

// The minimum effective screen dimensions supported by Windows are 800x600.
constexpr int kMinRdpScreenWidth = 800;
constexpr int kMinRdpScreenHeight = 600;

// Limit the RDP virtual display to 4K UHD (3840x2160 in landscape or
// 2160x3840 in portrait). Higher resolutions (e.g., 5K/6K from high-DPI
// clients) can cause Windows' indirect display driver (rdpidd.dll) to crash
// with protocol error 0x112f.
constexpr int kMaxRdpScreenWidth = 3840;
constexpr int kMaxRdpScreenHeight = 2160;

// Default dots per inch used by RDP is 96 DPI (100% scale, the minimum scale
// factor supported by the RDP ActiveX control's UpdateSessionDisplaySettings()
// API).
constexpr int kDefaultRdpDpi = 96;

}  // namespace

webrtc::DesktopSize GetBoundedRdpDesktopSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return webrtc::DesktopSize(kMinRdpScreenWidth, kMinRdpScreenHeight);
  }

  const webrtc::DesktopSize max_size =
      width >= height
          ? webrtc::DesktopSize(kMaxRdpScreenWidth, kMaxRdpScreenHeight)
          : webrtc::DesktopSize(kMaxRdpScreenHeight, kMaxRdpScreenWidth);
  webrtc::DesktopSize bounded_size =
      ScaleSizeToFit(webrtc::DesktopSize(width, height), max_size);

  // Ensure dimensions meet Windows' minimum effective desktop size (800x600)
  // and align to multiples of 2 (the RDP ActiveX control requires width to be
  // even, and I420 video codecs prefer even dimensions).
  return webrtc::DesktopSize(
      RoundToTwosMultiple(std::max(bounded_size.width(), kMinRdpScreenWidth)),
      RoundToTwosMultiple(
          std::max(bounded_size.height(), kMinRdpScreenHeight)));
}

ScreenResolution GetBoundedRdpResolution(const ScreenResolution& resolution) {
  // The RDP ActiveX control (IMsRdpClient8::UpdateSessionDisplaySettings) only
  // supports a single display with a uniform scale factor, and
  // RdpClientWindow::UpdateDesktopResolution() requires dpi.x() == dpi.y().
  int dpi = std::max(resolution.dpi().x(), kDefaultRdpDpi);

  const int width = resolution.dimensions().width();
  const int height = resolution.dimensions().height();
  const int max_width =
      width >= height ? kMaxRdpScreenWidth : kMaxRdpScreenHeight;
  const int max_height =
      width >= height ? kMaxRdpScreenHeight : kMaxRdpScreenWidth;
  if (width > max_width || height > max_height) {
    // Scale DPI down by the same factor used to fit `dimensions` into the 4K
    // limit so the effective logical desktop size is preserved, clamped to the
    // minimum 96 DPI (100% scale) supported by the RDP ActiveX control.
    int scaled_dpi = int64_t{max_width} * height <= int64_t{max_height} * width
                         ? MulDiv(dpi, max_width, width)
                         : MulDiv(dpi, max_height, height);
    dpi = std::max(kDefaultRdpDpi, scaled_dpi);
  }

  return ScreenResolution(GetBoundedRdpDesktopSize(width, height),
                          webrtc::DesktopVector(dpi, dpi));
}

// The core of RdpClient is ref-counted since it services calls and notifies
// events on the caller task runner, but runs the ActiveX control on the UI
// task runner.
class RdpClient::Core : public base::RefCountedThreadSafe<Core>,
                        public RdpClientWindow::EventHandler {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       RdpClient::EventHandler* event_handler);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

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
  raw_ptr<RdpClient::EventHandler> event_handler_;

  // Hosts the RDP ActiveX control.
  std::unique_ptr<RdpClientWindow> rdp_client_window_;

  // A self-reference to keep the object alive during connection shutdown.
  scoped_refptr<Core> self_;
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
      event_handler_(event_handler) {}

void RdpClient::Core::Connect(const ScreenResolution& resolution,
                              const std::string& terminal_id,
                              DWORD port_number) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::Connect, this, resolution, terminal_id,
                                  port_number));
    return;
  }

  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(!rdp_client_window_);
  DCHECK(!self_.get());

  net::IPEndPoint server_endpoint(net::IPAddress(kRdpLoopbackAddress),
                                  base::checked_cast<uint16_t>(port_number));

  // Create the ActiveX control window.
  rdp_client_window_ =
      std::make_unique<RdpClientWindow>(server_endpoint, terminal_id, this);
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

  if (event_handler_) {
    event_handler_->OnRdpConnected();
  }
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
