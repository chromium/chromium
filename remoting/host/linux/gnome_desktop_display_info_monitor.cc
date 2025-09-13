// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/linux/gnome_display_config.h"

namespace remoting {

GnomeDesktopDisplayInfoMonitor::GnomeDesktopDisplayInfoMonitor(
    base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor)
    : display_config_monitor_(display_config_monitor) {}

GnomeDesktopDisplayInfoMonitor::~GnomeDesktopDisplayInfoMonitor() = default;

void GnomeDesktopDisplayInfoMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_monitor_) {
    monitors_changed_subscription_ = display_config_monitor_->AddCallback(
        base::BindRepeating(
            &GnomeDesktopDisplayInfoMonitor::OnGnomeDisplayConfigReceived,
            base::Unretained(this)),
        /*call_with_current_config=*/true);
  }
}

void GnomeDesktopDisplayInfoMonitor::QueryDisplayInfo() {
  // This is a no-op, as the display info is pushed from
  // GnomeDisplayConfigMonitor.
}

void GnomeDesktopDisplayInfoMonitor::AddCallback(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_list_.AddUnsafe(std::move(callback));
}

void GnomeDesktopDisplayInfoMonitor::OnGnomeDisplayConfigReceived(
    const GnomeDisplayConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DesktopDisplayInfo info;
  switch (config.layout_mode) {
    case GnomeDisplayConfig::LayoutMode::kPhysical:
      info.set_pixel_type(DesktopDisplayInfo::PixelType::PHYSICAL);
      break;
    case GnomeDisplayConfig::LayoutMode::kLogical:
      info.set_pixel_type(DesktopDisplayInfo::PixelType::LOGICAL);
      break;
  }
  for (const auto& [name, monitor] : config.monitors) {
    const GnomeDisplayConfig::MonitorMode* current_mode =
        monitor.GetCurrentMode();
    if (!current_mode) {
      LOG(WARNING) << "Monitor " << name
                   << " ignored because it has no current mode";
      continue;
    }
    // current_mode->width/height are always in physical screen pixels, which
    // need to be divided by the monitor scale to get the logical pixels.
    int width = info.pixel_type() == DesktopDisplayInfo::PixelType::PHYSICAL
                    ? current_mode->width
                    : (current_mode->width / monitor.scale);
    int height = info.pixel_type() == DesktopDisplayInfo::PixelType::PHYSICAL
                     ? current_mode->height
                     : (current_mode->height / monitor.scale);
    // Ideally we should multiply the DPI with text-scaling-factor, but that
    // causes the client to resize the display to the actual screen resolution
    // at 1x scale when "High-DPI mode" is disabled.
    // TODO: crbug.com/431816005 - fix this bug on the host and set the the DPI
    // to `kDefaultDpi * monitor.scale * text_scaling_factor`.
    info.AddDisplay(DisplayGeometry(GnomeDisplayConfig::GetScreenId(name),
                                    monitor.x, monitor.y, width, height,
                                    kDefaultDpi * monitor.scale,
                                    /*bpp=*/24, monitor.is_primary, name));
  }
  callback_list_.Notify(info);
}

}  // namespace remoting
