// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_X11_COMMON_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_X11_COMMON_H_

#include "ui/events/devices/x11/xinput_util.h"
#include "ui/gfx/x/xinput.h"

namespace remoting {

// Returns the input event mask used by all the X11 local input monitors.
// Since the mask is set for the root window, each input monitor must use
// the same mask.
x11::Input::XIEventMask CommonXIEventMaskForRootWindow();

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_INPUT_MONITOR_X11_COMMON_H_
