// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_X11_H_
#define REMOTING_HOST_DESKTOP_RESIZER_X11_H_

#include <string.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "remoting/host/linux/scoped_glib.h"
#include "remoting/host/linux/x11_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/randr.h"

namespace remoting {

class X11CrtcResizer;

// Wrapper class for the XRRScreenResources struct.
class ScreenResources {
 public:
  ScreenResources();
  ~ScreenResources();

  bool Refresh(x11::RandR* randr, x11::Window window);

  x11::RandR::Mode GetIdForMode(const std::string& name);

  x11::RandR::GetScreenResourcesCurrentReply* get();

 private:
  std::unique_ptr<x11::RandR::GetScreenResourcesCurrentReply> resources_;
};

class DesktopResizerX11 : public DesktopResizer {
 public:
  DesktopResizerX11();
  DesktopResizerX11(const DesktopResizerX11&) = delete;
  DesktopResizerX11& operator=(const DesktopResizerX11&) = delete;
  ~DesktopResizerX11() override;

  // DesktopResizer interface
  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override;
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override;
  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;

 private:
  using OutputInfoList = std::vector<
      std::pair<x11::RandR::Output, x11::RandR::GetOutputInfoReply>>;

  // Add a mode matching the specified resolution and switch to it.
  void SetResolutionForOutput(x11::RandR::Output output,
                              const ScreenResolution& resolution);

  // Removes the existing mode from the output and replaces it with the new
  // size. Returns the new mode ID, or None (0) on failure.
  x11::RandR::Mode UpdateMode(x11::RandR::Output output, int width, int height);

  // Remove the specified mode from the output, and delete it. If the mode is in
  // use, it is not deleted.
  // |name| should be set to GetModeNameForOutput(output). The parameter is to
  // avoid creating the mode name twice.
  void DeleteMode(x11::RandR::Output output, const std::string& name);

  // Updates the root window using the bounding box of the CRTCs, then
  // re-activate all CRTCs.
  void UpdateRootWindow(X11CrtcResizer& resizer);

  // Gets a list of outputs that are not connected to any CRTCs.
  OutputInfoList GetDisabledOutputs();

  void RequestGnomeDisplayConfig();
  void OnGnomeDisplayConfigReceived(GnomeDisplayConfig config);

  raw_ptr<x11::Connection> connection_;
  const raw_ptr<x11::RandR> randr_ = nullptr;
  const raw_ptr<const x11::Screen> screen_ = nullptr;
  x11::Window root_;
  ScreenResources resources_;
  bool has_randr_;
  bool is_virtual_session_;

  // Used to fetch GNOME's current scale setting, so the correct
  // text-scaling-factor can be calculated.
  GnomeDisplayConfigDBusClient gnome_display_config_;

  // Used to rate-limit requests to GNOME.
  base::OneShotTimer gnome_delay_timer_;

  int requested_dpi_;

  // Used to set the text-scaling-factor.
  ScopedGObject<GSettings> registry_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_RESIZER_X11_H_
