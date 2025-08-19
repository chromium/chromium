// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"

#include "base/functional/bind.h"
#include "base/logging.h"

namespace remoting {

namespace {

constexpr int kDefaultDPI = 96;

}  // namespace

GnomeDesktopDisplayInfoMonitor::GnomeDesktopDisplayInfoMonitor(
    base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client)
    : display_config_client_(display_config_client) {}

GnomeDesktopDisplayInfoMonitor::~GnomeDesktopDisplayInfoMonitor() = default;

void GnomeDesktopDisplayInfoMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    monitors_changed_subscription_ =
        display_config_client_->SubscribeMonitorsChanged(base::BindRepeating(
            &GnomeDesktopDisplayInfoMonitor::QueryDisplayInfo,
            base::Unretained(this)));
  }
  QueryDisplayInfo();
}

void GnomeDesktopDisplayInfoMonitor::QueryDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    display_config_client_->GetMonitorsConfig(base::BindOnce(
        &GnomeDesktopDisplayInfoMonitor::OnGnomeDisplayConfigReceived,
        base::Unretained(this)));
  }
}

void GnomeDesktopDisplayInfoMonitor::AddCallback(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_list_.AddUnsafe(std::move(callback));
}

void GnomeDesktopDisplayInfoMonitor::OnGnomeDisplayConfigReceived(
    GnomeDisplayConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DesktopDisplayInfo info;
  for (const auto& [name, monitor] : config.monitors) {
    const GnomeDisplayConfig::MonitorMode* current_mode =
        monitor.GetCurrentMode();
    if (!current_mode) {
      LOG(WARNING) << "Monitor " << name
                   << " ignored because it has no current mode";
      continue;
    }
    // Ideally we should multiply the DPI with text-scaling-factor, but that
    // causes the client to resize the display to the actual screen resolution
    // at 1x scale when "High-DPI mode" is disabled.
    // TODO: crbug.com/431816005 - fix this bug on the host and set the the DPI
    // to `kDefaultDPI * monitor.scale * text_scaling_factor`.
    info.AddDisplay(DisplayGeometry(
        GnomeDisplayConfig::GetScreenId(name), monitor.x, monitor.y,
        current_mode->width, current_mode->height, kDefaultDPI * monitor.scale,
        /*bpp=*/24, monitor.is_primary, name));
  }
  callback_list_.Notify(info);
}

}  // namespace remoting
