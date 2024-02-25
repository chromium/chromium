// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader_x11.h"

#include <algorithm>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/x11_display_util.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/window_event_manager.h"

namespace remoting {

namespace {

// Monitors were added in XRANDR 1.5.
constexpr std::pair<uint32_t, uint32_t> kMinRandrVersion{1, 5};

}  // namespace

DesktopDisplayInfoLoaderX11::DesktopDisplayInfoLoaderX11() = default;

DesktopDisplayInfoLoaderX11::~DesktopDisplayInfoLoaderX11() {
  if (connection_) {
    connection_->RemoveEventObserver(this);
  }
}

void DesktopDisplayInfoLoaderX11::Init() {
  connection_ = x11::Connection::Get();
  connection_->AddEventObserver(this);
  randr_ = &connection_->randr();
  if (!randr_->present()) {
    HOST_LOG << "No XRANDR extension found.";
    return;
  }

  auto randr_version = connection_->randr_version();
  if (randr_version < kMinRandrVersion) {
    HOST_LOG << "XRANDR version (" << randr_version.first << ", "
             << randr_version.second << ") is too old.";
    return;
  }

  root_window_events_ = connection_->ScopedSelectEvent(
      ui::GetX11RootWindow(), x11::EventMask::StructureNotify);
  auto randr_event_mask =
      x11::RandR::NotifyMask::ScreenChange | x11::RandR::NotifyMask::CrtcChange;
  randr_->SelectInput({ui::GetX11RootWindow(), randr_event_mask});

  LoadMonitors();
}

DesktopDisplayInfo DesktopDisplayInfoLoaderX11::GetCurrentDisplayInfo() {
  DesktopDisplayInfo result;

  for (const auto& monitor : monitors_) {
    DisplayGeometry info;
    // webrtc::ScreenCapturerX11 uses the |name| Atom as the monitor ID.
    info.id = static_cast<int32_t>(monitor.name);
    info.is_default = monitor.primary;
    info.x = monitor.x;
    info.y = monitor.y;
    info.width = monitor.width;
    info.height = monitor.height;

    // Hard-code the default DPI instead of calculating it from the monitor's
    // resolution and physical size. This avoids an issue where the website
    // pre-multiplies the ClientResolution sizes by the host's pixels/DIPs
    // ratio, sometimes leading to a feedback loop of ever-increasing resizes.
    //
    // TODO: b/309174172 - Change this back to GetMonitorDpi(monitor).x() when
    // the website issue has been addressed.
    info.dpi = kDefaultDpi;
    info.bpp = 24;

    result.AddDisplay(info);
  }
  return result;
}

void DesktopDisplayInfoLoaderX11::OnEvent(const x11::Event& xevent) {
  const auto* configure_notify = xevent.As<x11::ConfigureNotifyEvent>();
  const auto* screen_change = xevent.As<x11::RandR::ScreenChangeNotifyEvent>();
  const auto* notify = xevent.As<x11::RandR::NotifyEvent>();
  if (configure_notify) {
    HOST_LOG << "Got X11 ConfigureNotify event.";
  } else if (screen_change) {
    HOST_LOG << "Got RANDR ScreenChange event.";
  } else if (notify) {
    HOST_LOG << "Got RANDR Notify event.";
  } else {
    // Unhandled event received on root window, ignore.
    return;
  }

  LoadMonitors();
}

void DesktopDisplayInfoLoaderX11::LoadMonitors() {
  if (connection_->randr_version() < kMinRandrVersion) {
    return;
  }

  auto reply = randr_->GetMonitors({ui::GetX11RootWindow()}).Sync();
  if (reply) {
    monitors_ = std::move(reply->monitors);
  } else {
    LOG(ERROR) << "RRGetMonitors request failed.";
  }
}

}  // namespace remoting
