// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/desktop_capturer.h"

namespace remoting {

bool DesktopCapturer::SupportsFrameCallbacks() {
  return false;
}

}  // namespace remoting
