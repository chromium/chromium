// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_loader.h"

namespace remoting {

namespace {

class DesktopDisplayInfoLoaderChromeOs : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderChromeOs() = default;
  ~DesktopDisplayInfoLoaderChromeOs() override = default;

  DesktopDisplayInfo GetCurrentDisplayInfo() override;
};

DesktopDisplayInfo DesktopDisplayInfoLoaderChromeOs::GetCurrentDisplayInfo() {
  // TODO(lambroslambrou): Implement this.
  return DesktopDisplayInfo();
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderChromeOs>();
}

}  // namespace remoting
