// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/curtain_mode_wayland.h"

namespace remoting {

CurtainModeWayland::CurtainModeWayland(bool is_headless)
    : is_headless_(is_headless) {}

CurtainModeWayland::~CurtainModeWayland() = default;

bool CurtainModeWayland::Activate() {
  return is_headless_;
}

}  // namespace remoting
