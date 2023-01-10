// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DESKTOP_DISPLAY_INFO_LOADER_WAYLAND_H_
#define REMOTING_HOST_LINUX_DESKTOP_DISPLAY_INFO_LOADER_WAYLAND_H_

#include "base/sequence_checker.h"
#include "remoting/host/desktop_display_info_loader.h"

namespace remoting {

class DesktopDisplayInfoLoaderWayland : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderWayland();

  // DesktopDisplayInfoLoader implementation.
  void Init() override;
  DesktopDisplayInfo GetCurrentDisplayInfo() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DESKTOP_DISPLAY_INFO_LOADER_WAYLAND_H_
