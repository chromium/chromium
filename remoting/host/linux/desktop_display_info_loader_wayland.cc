// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker.h"
#include "remoting/host/desktop_display_info_loader.h"

#include "remoting/host/linux/wayland_manager.h"

namespace remoting {

namespace {

class DesktopDisplayInfoLoaderWayland : public DesktopDisplayInfoLoader {
 public:
  DesktopDisplayInfoLoaderWayland();

  // DesktopDisplayInfoLoader implementation.
  void Init() override;
  DesktopDisplayInfo GetCurrentDisplayInfo() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

DesktopDisplayInfoLoaderWayland::DesktopDisplayInfoLoaderWayland() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void DesktopDisplayInfoLoaderWayland::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DesktopDisplayInfo DesktopDisplayInfoLoaderWayland::GetCurrentDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return WaylandManager::Get()->GetCurrentDisplayInfo();
}

}  // namespace

// static
std::unique_ptr<DesktopDisplayInfoLoader> DesktopDisplayInfoLoader::Create() {
  return std::make_unique<DesktopDisplayInfoLoaderWayland>();
}

}  // namespace remoting
