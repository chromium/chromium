// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CURTAIN_MODE_WAYLAND_H_
#define REMOTING_HOST_LINUX_CURTAIN_MODE_WAYLAND_H_

#include "remoting/host/curtain_mode.h"

namespace remoting {

class CurtainModeWayland : public CurtainMode {
 public:
  CurtainModeWayland();

  CurtainModeWayland(const CurtainModeWayland&) = delete;
  CurtainModeWayland& operator=(const CurtainModeWayland&) = delete;

  bool Activate() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CURTAIN_MODE_WAYLAND_H_
