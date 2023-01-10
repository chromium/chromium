// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_utils.h"
#include "base/environment.h"
#include "base/nix/xdg_util.h"

namespace remoting {

bool IsRunningWayland() {
  static base::nix::SessionType session_type =
      base::nix::GetSessionType(*base::Environment::Create());
  return session_type == base::nix::SessionType::kWayland;
}

}  // namespace remoting
