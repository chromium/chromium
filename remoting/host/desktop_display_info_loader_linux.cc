// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/notreached.h"

#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/linux/wayland_utils.h"

#if defined(REMOTING_USE_X11)
#include "remoting/host/desktop_display_info_loader_x11.h"
#endif

#if defined(REMOTING_USE_WAYLAND)
#include "remoting/host/linux/desktop_display_info_loader_wayland.h"
#endif

namespace remoting {

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  std::unique_ptr<DesktopDisplayInfoLoader> display_info_loader;
#if defined(REMOTING_USE_WAYLAND)
  if (IsRunningWayland()) {
    display_info_loader = std::make_unique<DesktopDisplayInfoLoaderWayland>();
  }
#elif defined(REMOTING_USE_X11)
  display_info_loader = std::make_unique<DesktopDisplayInfoLoaderX11>();
#else
#error "Should use either wayland or X11."
#endif
  return display_info_loader;
}

}  // namespace remoting
