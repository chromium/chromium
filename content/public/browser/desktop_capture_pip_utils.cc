// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_capture_pip_utils.h"

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/public/browser/browser_thread.h"

namespace content::desktop_capture {

std::optional<DesktopMediaID::Id> GetPipWindowToExcludeFromScreenCapture(
    DesktopMediaID::Id desktop_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    return coordinator->GetPipWindowToExcludeFromScreenCapture(desktop_id);
  }

  return std::nullopt;
}

void AddPipExclusionObserver(PipScreenCaptureExclusionObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    coordinator->AddExclusionObserver(observer);
  }
}

void RemovePipExclusionObserver(PipScreenCaptureExclusionObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    coordinator->RemoveExclusionObserver(observer);
  }
}

bool IsPipExcludedFromScreenCapture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* coordinator = PipScreenCaptureCoordinator::GetInstance()) {
    return coordinator->IsExcludedFromScreenCapture();
  }
  return false;
}

}  // namespace content::desktop_capture
