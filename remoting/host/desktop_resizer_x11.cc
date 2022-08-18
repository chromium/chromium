// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_x11.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/x11_util.h"
#include "remoting/host/x11_crtc_resizer.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/scoped_ignore_errors.h"

// On Linux, we use the xrandr extension to change the desktop resolution. In
// curtain mode, we do exact resize where supported. Otherwise, we try to pick
// the best resolution from the existing modes.
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

}  // namespace

namespace remoting {

ScreenResources::ScreenResources() = default;

ScreenResources::~ScreenResources() = default;

bool ScreenResources::Refresh(x11::RandR* randr, x11::Window window) {
  resources_ = nullptr;
  if (auto response = randr->GetScreenResourcesCurrent({window}).Sync())
    resources_ = std::move(response.reply);
  return resources_ != nullptr;
}

x11::RandR::Mode ScreenResources::GetIdForMode(const std::string& name) {
  CHECK(resources_);
  const char* names = reinterpret_cast<const char*>(resources_->names.data());
  for (const auto& mode_info : resources_->modes) {
    std::string mode_name(names, mode_info.name_len);
    names += mode_info.name_len;
    if (name == mode_name)
      return static_cast<x11::RandR::Mode>(mode_info.id);
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
      exact_resize_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          "server-supports-exact-resize")) {
  has_randr_ = randr_->present();
  if (!has_randr_)
    return;
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
  if (has_randr_)
    connection_->DispatchAll();

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
  if (!has_randr_)
    return result;
  if (exact_resize_) {
    // Clamp the specified size to something valid for the X server.
    if (auto response = randr_->GetScreenSizeRange({root_}).Sync()) {
      int width =
          base::clamp(static_cast<uint16_t>(preferred.dimensions().width()),
                      response->min_width, response->max_width);
      int height =
          base::clamp(static_cast<uint16_t>(preferred.dimensions().height()),
                      response->min_height, response->max_height);
      // Additionally impose a minimum size of 640x480, since anything smaller
      // doesn't seem very useful.
      ScreenResolution actual(
          webrtc::DesktopSize(std::max(640, width), std::max(480, height)),
          webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
      result.push_back(actual);
    }
  } else {
    // Retrieve supported resolutions with RandR
    if (auto response = randr_->GetScreenInfo({root_}).Sync()) {
      for (const auto& size : response->sizes) {
        result.emplace_back(webrtc::DesktopSize(size.width, size.height),
                            webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
      }
    }
  }
  return result;
}

void DesktopResizerX11::SetResolution(const ScreenResolution& resolution,
                                      webrtc::ScreenId screen_id) {
  if (!has_randr_)
    return;

  // Ignore X errors encountered while resizing the display. We might hit an
  // error, for example if xrandr has been used to add a mode with the same
  // name as our mode, or to remove it. We don't want to terminate the process
  // if this happens.
  x11::ScopedIgnoreErrors ignore_errors(connection_);

  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(connection_);

  if (!resources_.Refresh(randr_, root_))
    return;

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

    auto output = monitor.outputs[0];
    if (exact_resize_)
      SetResolutionNewMode(output, resolution);
    else
      SetResolutionExistingMode(resolution);
    return;
  }
  LOG(ERROR) << "Monitor " << screen_id << " not found.";
}

void DesktopResizerX11::RestoreResolution(const ScreenResolution& original,
                                          webrtc::ScreenId screen_id) {
  SetResolution(original, screen_id);
}

void DesktopResizerX11::SetResolutionNewMode(
    x11::RandR::Output output,
    const ScreenResolution& resolution) {
  // The name of the mode representing the current client view resolution. This
  // must be unique per Output, so that Outputs can be resized independently.
  std::string mode_name =
      "CRD_" + base::NumberToString(base::to_underlying(output));

  // Actually do the resize operation, preserving the current mode name. Note
  // that we have to detach the output from the mode in order to delete the
  // mode and re-create it with the new resolution. The output may also need to
  // be detached from all modes in order to reduce the root window size.
  HOST_LOG << "Changing desktop size to " << resolution.dimensions().width()
           << "x" << resolution.dimensions().height();

  X11CrtcResizer resizer(resources_.get(), randr_);

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

  DeleteMode(output, mode_name);
  auto mode = CreateMode(output, mode_name, resolution.dimensions().width(),
                         resolution.dimensions().height());
  if (mode == kInvalidMode) {
    // The CRTC is disabled, but there's no easy way to recover it here
    // (the mode it was attached to has gone).
    LOG(ERROR) << "Failed to create new mode.";
    return;
  }

  // Update |active_crtcs_| with new sizes and offsets.
  resizer.UpdateActiveCrtcs(crtc, mode, resolution.dimensions());

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

  // Apply the new CRTCs, which will re-enable any that were disabled.
  resizer.ApplyActiveCrtcs();
}

void DesktopResizerX11::SetResolutionExistingMode(
    const ScreenResolution& resolution) {
  if (auto config = randr_->GetScreenInfo({root_}).Sync()) {
    x11::RandR::Rotation current_rotation = config->rotation;
    const std::vector<x11::RandR::ScreenSize>& sizes = config->sizes;
    for (size_t i = 0; i < sizes.size(); ++i) {
      if (sizes[i].width == resolution.dimensions().width() &&
          sizes[i].height == resolution.dimensions().height()) {
        randr_->SetScreenConfig({
            .window = root_,
            .timestamp = x11::Time::CurrentTime,
            .config_timestamp = config->config_timestamp,
            .sizeID = static_cast<uint16_t>(i),
            .rotation = current_rotation,
            .rate = 0,
        });
        break;
      }
    }
  }
}

x11::RandR::Mode DesktopResizerX11::CreateMode(x11::RandR::Output output,
                                               const std::string& name,
                                               int width,
                                               int height) {
  x11::RandR::ModeInfo mode;
  mode.width = width;
  mode.height = height;
  mode.name_len = name.size();
  if (auto reply = randr_->CreateMode({root_, mode, name.c_str()}).Sync()) {
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

// static
std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return std::make_unique<DesktopResizerX11>();
}

}  // namespace remoting
