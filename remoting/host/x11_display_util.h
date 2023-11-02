// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_X11_DISPLAY_UTIL_H_
#define REMOTING_HOST_X11_DISPLAY_UTIL_H_

#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/x/randr.h"

namespace remoting {

// Calculates DPI from an X11 monitor object.
webrtc::DesktopVector GetMonitorDpi(const x11::RandR::MonitorInfo& monitor);

// Converts an X11 monitor object to VideoTrackLayout.
protocol::VideoTrackLayout ToVideoTrackLayout(
    const x11::RandR::MonitorInfo& monitor);

}  // namespace remoting

#endif  // REMOTING_HOST_X11_DISPLAY_UTIL_H_