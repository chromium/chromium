// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <memory>

#include "base/notreached.h"

#if defined(REMOTING_USE_X11)
#include "remoting/host/linux/desktop_resizer_x11.h"
#endif

namespace remoting {

// static
std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
#if defined(REMOTING_USE_X11)
  return std::make_unique<DesktopResizerX11>();
#else
#error "Invalid config detected."
#endif
}

}  // namespace remoting
