// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/desktop_display_info_loader_wayland.h"

#include "base/sequence_checker.h"
#include "remoting/host/linux/wayland_manager.h"

namespace remoting {

DesktopDisplayInfoLoaderWayland::DesktopDisplayInfoLoaderWayland() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void DesktopDisplayInfoLoaderWayland::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DesktopDisplayInfo DesktopDisplayInfoLoaderWayland::GetCurrentDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return WaylandManager::Get()->GetCurrentDisplayInfo();
}

}  // namespace remoting
