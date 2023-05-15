// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_x11.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_display_layout_util.h"
#include "remoting/host/linux/x11_util.h"
#include "remoting/host/x11_crtc_resizer.h"
#include "remoting/host/x11_display_util.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"

// On Linux, we use the xrandr extension to change the desktop resolution.
//
// Xrandr has a number of restrictions that make exact resize more complex:
//
//   1. It's not possible to change the resolution of an existing mode. Instead,
//      the mode must be deleted and recreated.
//   2. It's not possible to delete a mode that's in use.
//   3. Errors are communicated via Xlib's spectacularly unhelpful mechanism
//      of terminating the process unless you install an error handler.
//   4. The root window size must always enclose any enabled Outputs (that is,
//      any output which is attached to a CRTC).
//   5. An Output cannot be given properties (xy-offsets, mode) which would
//      extend its rectangle beyond the root window size.
//
// Since we want the current mode name to be consistent (for each Output), the
// approach is as follows:
//
//   1. Fetch information about all the active (enabled) CRTCs.
//   2. Disable the RANDR Output being resized.
//   3. Delete the CRD mode, if it exists.
//   4. Create the CRD mode at the new resolution, and add it to the Output's
//      list of modes.
//   5. Adjust the properties (in memory) of any CRTCs to be modified:
//      * Width/height (mode) of the CRTC being resized.
//      * xy-offsets to avoid overlapping CRTCs.
//   6. Disable any CRTCs that might prevent changing the root window size.
//   7. Compute the bounding rectangle of all CRTCs (after adjustment), and set
//      it as the new root window size.
//   8. Apply all adjusted CRTC properties to the CRTCs. This will set the
//      Output being resized to the new CRD mode (which re-enables it), and it
//      will re-enable any other CRTCs that were disabled.

namespace {

constexpr auto kInvalidMode = static_cast<x11::RandR::Mode>(0);
constexpr auto kDisabledCrtc = static_cast<x11::RandR::Crtc>(0);

int PixelsToMillimeters(int pixels, int dpi) {
  DCHECK(dpi != 0);

  const double kMillimetersPerInch = 25.4;

  // (pixels / dpi) is the length in inches. Multiplying by
  // kMillimetersPerInch converts to mm. Multiplication is done first to
  // avoid integer division.
  return static_cast<int>(kMillimetersPerInch * pixels / dpi);
}

// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;

x11::RandR::Output GetOutputFromContext(void* context) {
  return reinterpret_cast<x11::RandR::MonitorInfo*>(context)->outputs[0];
}

std::string GetModeNameForOutput(x11::RandR::Output output) {
  // The name of the mode representing the current client view resolution. This
  // must be unique per Output, so that Outputs can be resized independently.
  return "CRD_" + base::NumberToString(base::to_underlying(output));
}

uint32_t GetDotClockForModeInfo() {
  static int proc_num = base::SysInfo::NumberOfProcessors();
  // Keep the proc_num logic in sync with linux_me2me_host.py
  if (proc_num > 16) {
    return 120 * 1e6;
  }
  return 60 * 1e6;
}

}  // namespace

namespace remoting {

ScreenResources::ScreenResources() = default;

ScreenResources::~ScreenResources() = default;

bool ScreenResources::Refresh(x11::RandR* randr, x11::Window window) {
  resources_ = nullptr;
  if (auto response = randr->GetScreenResourcesCurrent({window}).Sync()) {
    resources_ = std::move(response.reply);
  }
  return resources_ != nullptr;
}

x11::RandR::Mode ScreenResources::GetIdForMode(const std::string& name) {
  CHECK(resources_);
  const char* names = reinterpret_cast<const char*>(resources_->names.data());
  for (const auto& mode_info : resources_->modes) {
    std::string mode_name(names, mode_info.name_len);
    names += mode_info.name_len;
    if (name == mode_name) {
      return static_cast<x11::RandR::Mode>(mode_info.id);
    }
  }
  return kInvalidMode;
}

x11::RandR::GetScreenResourcesCurrentReply* ScreenResources::get() {
  return resources_.get();
}

DesktopResizerX11::DesktopResizerX11()
    : connection_(x11::Connection::Get()),
      randr_(&connection_->randr()),
      screen_(&connection_->default_screen()),
      root_(screen_->root),
      is_virtual_session_(IsVirtualSession(connection_)) {
  has_randr_ = randr_->present();
  if (!has_randr_) {
    return;
  }
  // Let the server know the client version so it sends us data consistent with
  // xcbproto's definitions.  We don't care about the returned server version,
  // so no need to sync.
  randr_->QueryVersion({x11::RandR::major_version, x11::RandR::minor_version});
  randr_->SelectInput({root_, x11::RandR::NotifyMask::ScreenChange});
}

DesktopResizerX11::~DesktopResizerX11() = default;

ScreenResolution DesktopResizerX11::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  // Process pending events so that the connection setup data is updated
  // with the correct display metrics.
  if (has_randr_) {
    connection_->DispatchAll();
  }

  // RANDR does not allow fetching information on a particular monitor. So
  // fetch all of them and try to find the requested monitor.
  auto reply = randr_->GetMonitors({root_}).Sync();
  if (reply) {
    for (const auto& monitor : reply->monitors) {
      if (static_cast<webrtc::ScreenId>(monitor.name) != screen_id) {
        continue;
      }
      return ScreenResolution(
          webrtc::DesktopSize(monitor.width, monitor.height),
          GetMonitorDpi(monitor));
    }
  }

  LOG(ERROR) << "Cannot find current resolution for screen ID " << screen_id
             << ". Resolution of the default screen will be returned.";

  ScreenResolution result(
      webrtc::DesktopSize(connection_->default_screen().width_in_pixels,
                          connection_->default_screen().height_in_pixels),
      webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
  return result;
}

std::list<ScreenResolution> DesktopResizerX11::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  std::list<ScreenResolution> result;
  if (!has_randr_ || !is_virtual_session_) {
    return result;
  }

  // Clamp the specified size to something valid for the X server.
  if (auto response = randr_->GetScreenSizeRange({root_}).Sync()) {
    int width =
        std::clamp(static_cast<uint16_t>(preferred.dimensions().width()),
                   response->min_width, response->max_width);
    int height =
        std::clamp(static_cast<uint16_t>(preferred.dimensions().height()),
                   response->min_height, response->max_height);
    // Additionally impose a minimum size of 640x480, since anything smaller
    // doesn't seem very useful.
    ScreenResolution actual(
        webrtc::DesktopSize(std::max(640, width), std::max(480, height)),
        webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
    result.push_back(actual);
  }
  return result;
}

void DesktopResizerX11::SetResolution(const ScreenResolution& resolution,
                                      webrtc::ScreenId screen_id) {
  if (!has_randr_ || !is_virtual_session_) {
    return;
  }

  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(connection_);

  if (!resources_.Refresh(randr_, root_)) {
    return;
  }

  // RANDR does not allow fetching information on a particular monitor. So
  // fetch all of them and try to find the requested monitor.
  auto reply = randr_->GetMonitors({root_}).Sync();
  if (!reply) {
    return;
  }

  for (const auto& monitor : reply->monitors) {
    if (static_cast<webrtc::ScreenId>(monitor.name) != screen_id) {
      continue;
    }

    if (monitor.outputs.size() != 1) {
      // This implementation only supports resizing a Monitor attached to a
      // single output. The case where size() > 1 should never occur with
      // Xorg+video-dummy.
      // TODO(crbug.com/1326339): Maybe support resizing a Monitor not
      // attached to any Output?
      LOG(ERROR) << "Monitor " << screen_id
                 << " has unexpected #outputs: " << monitor.outputs.size();
      return;
    }

    if (!monitor.automatic) {
      // This implementation only supports resizing synthesized Monitors which
      // automatically track their Outputs.
      // TODO(crbug.com/1326339): Maybe support resizing manually-created
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

  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(connection_);

  if (!resources_.Refresh(randr_, root_)) {
    return;
  }

  auto reply = randr_->GetMonitors({root_}).Sync();
  if (!reply) {
    return;
  }

  std::vector<VideoTrackLayoutWithContext> current_displays;
  for (auto& monitor : reply->monitors) {
    // This implementation only supports resizing synthesized Monitors which
    // automatically track their Outputs.
    // TODO(crbug.com/1326339): Maybe support resizing manually-created
    // monitors?
    if (monitor.automatic) {
      current_displays.push_back(
          {.layout = ToVideoTrackLayout(monitor), .context = &monitor});
    }
  }
  // TODO(yuweih): Verify that the layout is valid, e.g. no overlaps or gaps
  // between displays.
  DisplayLayoutDiff diff = CalculateDisplayLayoutDiff(current_displays, layout);

  X11CrtcResizer resizer(resources_.get(), connection_);
  resizer.FetchActiveCrtcs();

  // Add displays
  if (!diff.new_displays.empty()) {
    auto outputs = GetDisabledOutputs();
    size_t i = 0u;
    for (; i < outputs.size() && i < diff.new_displays.size(); i++) {
      auto& output_pair = outputs[i];
      auto output = output_pair.first;
      auto& output_info = output_pair.second;
      // For the video-dummy driver, the size of |crtcs| is exactly 1 and is
      // different for each Output. In general, this is not true for other
      // video-drivers, and the lists can overlap.
      // TODO(yuweih): Consider making CRTC allocation smarter so it works with
      // non-video-dummy drivers.
      if (output_info.crtcs.empty()) {
        LOG(ERROR) << "No available CRTC found associated with "
                   << reinterpret_cast<char*>(output_info.name.data());
        continue;
      }
      auto crtc = output_info.crtcs.front();
      auto track_layout = diff.new_displays[i];
      // Note that this has a weird behavior in GNOME, such that, if |output| is
      // "disconnected", creating the mode somehow resizes all existing displays
      // to 1024x768. Once the output is successfully enabled, it will remain
      // "connected" and will no longer have the problem. The problem doesn't
      // occur on XFCE or Cinnamon.
      // TODO(yuweih): See if this is fixable, or at least implement some
      // workaround, such as re-applying the layout.
      auto mode =
          UpdateMode(output, track_layout.width(), track_layout.height());
      if (mode == kInvalidMode) {
        LOG(ERROR) << "Failed to create new mode.";
        continue;
      }
      resizer.AddActiveCrtc(
          crtc, mode, {output},
          webrtc::DesktopRect::MakeXYWH(
              track_layout.position_x(), track_layout.position_y(),
              track_layout.width(), track_layout.height()));
      HOST_LOG << "Added display with crtc: " << base::to_underlying(crtc)
               << ", output: " << base::to_underlying(output);
    }
    if (i < diff.new_displays.size()) {
      LOG(WARNING) << "Failed to create " << (diff.new_displays.size() - i)
                   << " display(s) due to insufficient resources.";
    }
  }

  // Update displays
  for (const auto& updated_display : diff.updated_displays) {
    auto track_layout = updated_display.layout;
    auto output = GetOutputFromContext(updated_display.context);
    auto crtc = resizer.GetCrtcForOutput(output);
    if (crtc == kDisabledCrtc) {
      // This is not expected to happen. Disabled Outputs are not expected to
      // have any Monitor, but |output| was found in the RRGetMonitors response,
      // so it should have a CRTC attached.
      LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
      continue;
    }
    resizer.DisableCrtc(crtc);
    auto mode = UpdateMode(output, track_layout.width(), track_layout.height());
    if (mode == kInvalidMode) {
      LOG(ERROR) << "Failed to create new mode.";
      continue;
    }
    resizer.UpdateActiveCrtc(
        crtc, mode,
        webrtc::DesktopRect::MakeXYWH(
            track_layout.position_x(), track_layout.position_y(),
            track_layout.width(), track_layout.height()));
    HOST_LOG << "Updated display with screen ID: "
             << updated_display.layout.screen_id();
  }

  // Remove displays
  for (const auto& removed_display : diff.removed_displays) {
    auto output = GetOutputFromContext(removed_display.context);
    auto crtc = resizer.GetCrtcForOutput(output);
    if (crtc == kDisabledCrtc) {
      LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
      continue;
    }
    resizer.DisableCrtc(crtc);
    resizer.RemoveActiveCrtc(crtc);
    DeleteMode(output, GetModeNameForOutput(output));
    HOST_LOG << "Removed display with screen ID: "
             << removed_display.layout.screen_id();
  }

  resizer.NormalizeCrtcs();
  UpdateRootWindow(resizer);
}

void DesktopResizerX11::SetResolutionForOutput(
    x11::RandR::Output output,
    const ScreenResolution& resolution) {
  // Actually do the resize operation, preserving the current mode name. Note
  // that we have to detach the output from the mode in order to delete the
  // mode and re-create it with the new resolution. The output may also need to
  // be detached from all modes in order to reduce the root window size.
  HOST_LOG << "Changing desktop size to " << resolution.dimensions().width()
           << "x" << resolution.dimensions().height();

  X11CrtcResizer resizer(resources_.get(), connection_);

  resizer.FetchActiveCrtcs();
  auto crtc = resizer.GetCrtcForOutput(output);

  if (crtc == kDisabledCrtc) {
    // This is not expected to happen. Disabled Outputs are not expected to
    // have any Monitor, but |output| was found in the RRGetMonitors response,
    // so it should have a CRTC attached.
    LOG(ERROR) << "No CRTC found for output: " << base::to_underlying(output);
    return;
  }

  // Disable the output now, so that the old mode can be deleted and the new
  // mode created and added to the output's available modes. The previous size
  // and offsets will be stored in |resizer|.
  resizer.DisableCrtc(crtc);

  auto mode = UpdateMode(output, resolution.dimensions().width(),
                         resolution.dimensions().height());
  if (mode == kInvalidMode) {
    // The CRTC is disabled, but there's no easy way to recover it here
    // (the mode it was attached to has gone).
    LOG(ERROR) << "Failed to create new mode.";
    return;
  }

  // Update |active_crtcs_| with new sizes and offsets.
  resizer.UpdateActiveCrtcs(crtc, mode, resolution.dimensions());
  UpdateRootWindow(resizer);
}

x11::RandR::Mode DesktopResizerX11::UpdateMode(x11::RandR::Output output,
                                               int width,
                                               int height) {
  std::string mode_name = GetModeNameForOutput(output);
  DeleteMode(output, mode_name);

  // Set some clock values so that the computed refresh-rate is a realistic
  // number:
  // 60Hz = dot_clock / (htotal * vtotal).
  // This allows GNOME's Display Settings tool to apply new settings for
  // resolution/scaling - see crbug.com/1374488.
  x11::RandR::ModeInfo mode;
  mode.width = width;
  mode.height = height;
  mode.dot_clock = GetDotClockForModeInfo();
  mode.htotal = 1000;
  mode.vtotal = 1000;
  mode.name_len = mode_name.size();
  if (auto reply =
          randr_->CreateMode({root_, mode, mode_name.c_str()}).Sync()) {
    randr_->AddOutputMode({
        output,
        reply->mode,
    });
    return reply->mode;
  }
  return kInvalidMode;
}

void DesktopResizerX11::DeleteMode(x11::RandR::Output output,
                                   const std::string& name) {
  x11::RandR::Mode mode_id = resources_.GetIdForMode(name);
  if (mode_id != kInvalidMode) {
    randr_->DeleteOutputMode({output, mode_id});
    randr_->DestroyMode({mode_id});
    resources_.Refresh(randr_, root_);
  }
}

void DesktopResizerX11::UpdateRootWindow(X11CrtcResizer& resizer) {
  // Disable any CRTCs that have been changed, so that the root window can be
  // safely resized to the bounding-box of the new CRTCs.
  // This is non-optimal: the only CRTCs that need disabling are those whose
  // original rectangles don't fit into the new root window - they are the ones
  // that would prevent resizing the root window. But figuring these out would
  // involve keeping track of all the original rectangles as well as the new
  // ones. So, to keep the implementation simple (and working for any arbitrary
  // layout algorithm), all changed CRTCs are disabled here.
  resizer.DisableChangedCrtcs();

  // Get the dimensions to resize the root window to.
  auto dimensions = resizer.GetBoundingBox();

  // TODO(lambroslambrou): Use the DPI from client size information.
  uint32_t width_mm = PixelsToMillimeters(dimensions.width(), kDefaultDPI);
  uint32_t height_mm = PixelsToMillimeters(dimensions.height(), kDefaultDPI);
  randr_->SetScreenSize({root_, static_cast<uint16_t>(dimensions.width()),
                         static_cast<uint16_t>(dimensions.height()), width_mm,
                         height_mm});

  resizer.MoveApplicationWindows();

  // Apply the new CRTCs, which will re-enable any that were disabled.
  resizer.ApplyActiveCrtcs();
}

DesktopResizerX11::OutputInfoList DesktopResizerX11::GetDisabledOutputs() {
  OutputInfoList disabled_outputs;
  for (x11::RandR::Output output : resources_.get()->outputs) {
    auto reply = randr_
                     ->GetOutputInfo({.output = output,
                                      .config_timestamp =
                                          resources_.get()->config_timestamp})
                     .Sync();
    if (!reply) {
      continue;
    }
    if (reply->crtc == kDisabledCrtc) {
      disabled_outputs.emplace_back(output, std::move(*reply.reply));
    }
  }
  return disabled_outputs;
}

}  // namespace remoting
