// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"

#include <memory>

#include "base/notreached.h"

#if defined(USE_X11)
#include "remoting/host/desktop_resizer_x11.h"
#endif

#if defined(USE_OZONE)
#include "remoting/host/desktop_resizer_ozone.h"
#include "ui/base/ui_base_features.h"
#endif

namespace remoting {

std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return std::make_unique<DesktopResizerOzone>();
#endif
#if defined(USE_X11)
  return std::make_unique<DesktopResizerX11>();
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace remoting
