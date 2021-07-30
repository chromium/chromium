// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

namespace remoting {

namespace {

class DesktopDisplayInfoLoaderX11 : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderX11() = default;
  ~DesktopDisplayInfoLoaderX11() override = default;

  void Init() override;
  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

void DesktopDisplayInfoLoaderX11::Init() {
  // TODO(lambroslambrou): Initialize X11 and RANDR.
}

DesktopDisplayInfo DesktopDisplayInfoLoaderX11::GetCurrentDisplayInfo() {
  // TODO(lambroslambrou): Return a list of monitors which is consistent with
  // the existing multi-monitor support in webrtc::ScreenCapturerX11.
  return DesktopDisplayInfo();
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderX11>();
}

}  // namespace remoting
