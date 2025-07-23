// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/curtain_mode_wayland.h"

namespace remoting {

CurtainModeWayland::CurtainModeWayland() = default;

bool CurtainModeWayland::Activate() {
  // Wayland support is only implemented for headless sessions which are
  // already curtained.
  return true;
}

}  // namespace remoting
