// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/notreached.h"

#include "remoting/host/desktop_display_info_loader.h"
#include "remoting/host/desktop_display_info_loader_x11.h"
#include "remoting/host/linux/desktop_display_info_loader_wayland.h"
#include "remoting/host/linux/wayland_utils.h"

namespace remoting {

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  if (IsRunningWayland()) {
    return std::make_unique<DesktopDisplayInfoLoaderWayland>();
  }
  return std::make_unique<DesktopDisplayInfoLoaderX11>();
}

}  // namespace remoting
