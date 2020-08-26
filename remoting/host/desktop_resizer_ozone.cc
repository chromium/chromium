// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer_ozone.h"

#include "build/build_config.h"

namespace remoting {

DesktopResizerOzone::DesktopResizerOzone() = default;

DesktopResizerOzone::~DesktopResizerOzone() = default;

ScreenResolution DesktopResizerOzone::GetCurrentResolution() {
  NOTIMPLEMENTED();
  return ScreenResolution();
}

std::list<ScreenResolution> DesktopResizerOzone::GetSupportedResolutions(
    const ScreenResolution& preferred) {
  NOTIMPLEMENTED();
  return std::list<ScreenResolution>();
}

void DesktopResizerOzone::SetResolution(const ScreenResolution& resolution) {
  NOTIMPLEMENTED();
}

void DesktopResizerOzone::RestoreResolution(const ScreenResolution& original) {}

// To avoid multiple definitions when use_x11 && use_ozone is true, disable this
// factory method for OS_LINUX as Linux has a factory method that decides what
// desktopresizer to use based on IsUsingOzonePlatform feature flag.
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
std::unique_ptr<DesktopResizer> DesktopResizer::Create() {
  return base::WrapUnique(new DesktopResizerOzone);
}
#endif

}  // namespace remoting
