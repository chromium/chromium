// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture_pip_utils.h"

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"

namespace content::desktop_capture {

std::optional<DesktopMediaID::Id> GetPipWindowToExcludeFromScreenCapture(
    DesktopMediaID::Id desktop_id) {
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    return coordinator->GetPipWindowToExcludeFromScreenCapture(desktop_id);
  }

  return std::nullopt;
}

}  // namespace content::desktop_capture
