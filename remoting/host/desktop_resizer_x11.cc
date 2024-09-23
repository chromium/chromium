// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_x11.h"

#include <gio/gio.h>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_geometry.h"
#include "remoting/host/linux/x11_util.h"
#include "remoting/host/x11_display_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/x11_crtc_resizer.h"

namespace remoting {

namespace {

// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;
constexpr base::TimeDelta kGnomeWaitTime = base::Seconds(1);

uint32_t GetDotClockForModeInfo() {
  static int proc_num = base::SysInfo::NumberOfProcessors();
  // Keep the proc_num logic in sync with linux_me2me_host.py
  if (proc_num > 16) {
    return 120 * 1e6;
  }
  return 60 * 1e6;
}

}  // namespace

DesktopResizerX11::DesktopResizerX11()
    : connection_(x11::Connection::Get()),
      randr_output_manager_("CRD_", GetDotClockForModeInfo()),
      is_virtual_session_(IsVirtualSession(connection_)) {
  has_randr_ = RandR()->present();
  if (!has_randr_) {
    return;
  }
  RandR()->SelectInput({RootWindow(), x11::RandR::NotifyMask::ScreenChange});

  gnome_display_config_.Init();
  registry_ = TakeGObject(g_settings_new("org.gnome.desktop.interface"));
}

DesktopResizerX11::~DesktopResizerX11() = default;

// DesktopResizer interface
ScreenResolution DesktopResizerX11::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  // Process pending events so that the connection setup data is updated
  // with the correct display metrics.
  if (has_randr_) {
    connection_->DispatchAll();
  }

  // RANDR does not allow fetching information on a particular monitor. So
  // fetch all of them and try to find the requested monitor.
  auto reply = RandR()->GetMonitors({RootWindow()}).Sync();
  if (reply) {
    for (const auto& monitor : reply->monitors) {
      if (static_cast<x11::RandRMonitorConfig::ScreenId>(monitor.name) !=
          static_cast<x11::RandRMonitorConfig::ScreenId>(screen_id)) {
        continue;
      }
      gfx::Vector2d dpi = GetMonitorDpi(monitor);
      return ScreenResolution(
          webrtc::DesktopSize(monitor.width, monitor.height),
          webrtc::DesktopVector(dpi.x(), dpi.y()));
    }
  }

  LOG(ERROR) << "Cannot find current resolution for screen ID " << screen_id
             << ". Resolution of the default screen will be returned.";

  return ScreenResolution(
      webrtc::DesktopSize(connection_->default_screen().width_in_pixels,
                          connection_->default_screen().height_in_pixels),
      webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
}
std::list<ScreenResolution> DesktopResizerX11::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  std::list<ScreenResolution> result;
  if (!has_randr_ || !is_virtual_session_) {
    return result;
  }

  // Clamp the specified size to something valid for the X server.
  if (auto response = RandR()->GetScreenSizeRange({RootWindow()}).Sync()) {
    int width =
        std::clamp(static_cast<uint16_t>(preferred.dimensions().width()),
                   response->min_width, response->max_width);
    int height =
        std::clamp(static_cast<uint16_t>(preferred.dimensions().height()),
                   response->min_height, response->max_height);
    // Additionally impose a minimum size of 640x480, since anything smaller
    // doesn't seem very useful.
    result.emplace_back(
        webrtc::DesktopSize(std::max(640, width), std::max(480, height)),
        preferred.dpi());
  }
  return result;
}
void DesktopResizerX11::SetResolution(const ScreenResolution& resolution,
                                      webrtc::ScreenId screen_id) {
  if (!has_randr_ || !is_virtual_session_) {
    return;
  }

  // Grab the X server while we're changing the display resolution. This
  // ensures that the display configuration doesn't change under our feet.
  x11::ScopedXGrabServer grabber(connection_);

  // RANDR does not allow fetching information on a particular monitor. So
  // fetch all of them and try to find the requested monitor.
  std::vector<x11::RandR::MonitorInfo> monitors;
  if (!randr_output_manager_.TryGetCurrentMonitors(monitors)) {
    return;
  }

  for (const auto& monitor : monitors) {
    if (static_cast<x11::RandRMonitorConfig::ScreenId>(monitor.name) !=
        static_cast<x11::RandRMonitorConfig::ScreenId>(screen_id)) {
      continue;
    }

    if (monitor.outputs.size() != 1) {
      // This implementation only supports resizing a Monitor attached to a
      // single output. The case where size() > 1 should never occur with
      // Xorg+video-dummy.
      // TODO(crbug.com/40225767): Maybe support resizing a Monitor not
      // attached to any Output?
      LOG(ERROR) << "Monitor " << screen_id
                 << " has unexpected #outputs: " << monitor.outputs.size();
      return;
    }

    if (!monitor.automatic) {
      // This implementation only supports resizing synthesized Monitors which
      // automatically track their Outputs.
      // TODO(crbug.com/40225767): Maybe support resizing manually-created
      // Monitors?
      LOG(ERROR) << "Not resizing Monitor " << screen_id
                 << " that was created manually.";
      return;
    }

    SetResolutionForOutput(monitor.outputs[0], resolution);
    return;
  }
  LOG(ERROR) << "Monitor " << screen_id << " not found.";
}

void DesktopResizerX11::RestoreResolution(const ScreenResolution& original,
                                          webrtc::ScreenId screen_id) {
  SetResolution(original, screen_id);
}

void DesktopResizerX11::SetVideoLayout(const protocol::VideoLayout& layout) {
  if (!has_randr_ || !is_virtual_session_) {
    return;
  }
  x11::RandRMonitorLayout desktop_layouts;
  if (layout.has_primary_screen_id()) {
    desktop_layouts.primary_screen_id = layout.primary_screen_id();
  }
  for (const auto& track : layout.video_track()) {
    desktop_layouts.configs.emplace_back(
        track.has_screen_id() ? std::make_optional(track.screen_id())
                              : std::nullopt,
        gfx::Rect(track.position_x(), track.position_y(), track.width(),
                  track.height()),
        gfx::Vector2d(track.x_dpi(), track.y_dpi()));
  }
  randr_output_manager_.SetLayout(desktop_layouts);
}

void DesktopResizerX11::SetResolutionForOutput(
    x11::RandR::Output output,
    const ScreenResolution& resolution) {
  // Actually do the resize operation, preserving the current mode name. Note
  // that we have to detach the output from the mode in order to delete the
  // mode and re-create it with the new resolution. The output may also need to
  // be detached from all modes in order to reduce the root window size.
  HOST_LOG << "Resizing RANDR Output " << base::to_underlying(output) << " to "
           << resolution.dimensions().width() << "x"
           << resolution.dimensions().height();

  randr_output_manager_.SetResolutionForOutput(
      output,
      gfx::Size(resolution.dimensions().width(),
                resolution.dimensions().height()),
      gfx::Vector2d(resolution.dpi().x(), resolution.dpi().y()));

  // Check to see if GNOME is using automatic-scaling. If the value is non-zero,
  // the user prefers a particular scaling, so don't adjust the
  // text-scaling-factor here.
  if (g_settings_get_uint(registry_.get(), "scaling-factor") == 0U) {
    // Start the timer to update the text-scaling-factor. Any previously
    // started timer will be cancelled.
    requested_dpi_ = resolution.dpi().x();
    gnome_delay_timer_.Start(FROM_HERE, kGnomeWaitTime, this,
                             &DesktopResizerX11::RequestGnomeDisplayConfig);
  }
}

void DesktopResizerX11::RequestGnomeDisplayConfig() {
  // Unretained() is safe because `this` owns gnome_display_config_ which
  // cancels callbacks on destruction.
  gnome_display_config_.GetMonitorsConfig(
      base::BindOnce(&DesktopResizerX11::OnGnomeDisplayConfigReceived,
                     base::Unretained(this)));
}

void DesktopResizerX11::OnGnomeDisplayConfigReceived(
    GnomeDisplayConfig config) {
  // Look for an enabled monitor. Disabled monitors have no Mode set - a
  // monitor can become disabled by being added then removed (using the website
  // Display options). The Xorg xf86-video-dummy driver has a quirk that, once a
  // monitor becomes "connected", it stays forever in the connected state, even
  // if it is later disabled. All connected monitors (enabled or disabled) are
  // included in the GNOME config.

  // For X11, the calculation of the text-scaling-factor does not depend on
  // which enabled monitor is chosen here, because GNOME's X11 backend forces
  // all monitors to have the same scale. However, it makes sense to select
  // an enabled monitor, since a disabled monitor might not have a reliable
  // "scale" property returned by GNOME.
  auto monitor_iter =
      base::ranges::find_if(config.monitors, [](const auto& entry) {
        return entry.second.GetCurrentMode() != nullptr;
      });
  if (monitor_iter == std::ranges::end(config.monitors)) {
    LOG(ERROR) << "No enabled monitor found in GNOME config.";
    return;
  }
  const auto& monitor = monitor_iter->second;

  if (monitor.scale == 0) {
    // This should never happen - avoid division by 0.
    return;
  }

  // The GNOME scaling, multiplied by the GNOME text-scaling-factor, will be the
  // rendered scaling of text. This should be the client's requested DPI divided
  // by kDefaultDPI.
  double text_scaling_factor =
      static_cast<double>(requested_dpi_) / kDefaultDPI / monitor.scale;
  HOST_LOG << "Target DPI = " << requested_dpi_
           << ", GNOME scale = " << monitor.scale
           << ", calculated text-scaling = " << text_scaling_factor;

  if (!g_settings_set_double(registry_.get(), "text-scaling-factor",
                             text_scaling_factor)) {
    // Just log a warning - failure is expected if the value falls outside the
    // interval [0.5, 3.0].
    LOG(WARNING) << "Failed to set text-scaling-factor.";
  }
}

}  // namespace remoting
