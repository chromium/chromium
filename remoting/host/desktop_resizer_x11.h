// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_RESIZER_X11_H_
#define REMOTING_HOST_DESKTOP_RESIZER_X11_H_

#include "base/timer/timer.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/randr_output_manager.h"

namespace remoting {

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

  // Gets a list of outputs that are not connected to any CRTCs.
  OutputInfoList GetDisabledOutputs();

  void RequestGnomeDisplayConfig();
  void OnGnomeDisplayConfigReceived(GnomeDisplayConfig config);

  x11::RandR* RandR() const { return &connection_->randr(); }
  const x11::Window& RootWindow() const {
    return connection_->default_screen().root;
  }

  raw_ptr<x11::Connection> connection_;
  x11::RandROutputManager randr_output_manager_;
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
