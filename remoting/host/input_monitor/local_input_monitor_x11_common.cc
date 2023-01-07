// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor_x11_common.h"

namespace remoting {

x11::Input::XIEventMask CommonXIEventMaskForRootWindow() {
  x11::Input::XIEventMask mask{};
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyPress);
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawKeyRelease);
  ui::SetXinputMask(&mask, x11::Input::RawDeviceEvent::RawMotion);
  return mask;
}

}  // namespace remoting
