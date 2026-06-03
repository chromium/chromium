// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_pip_screen_capture_coordinator.h"

namespace content {

TestPipScreenCaptureCoordinator::TestPipScreenCaptureCoordinator(
    bool is_excluded)
    : is_excluded_(is_excluded) {}

TestPipScreenCaptureCoordinator::~TestPipScreenCaptureCoordinator() = default;

std::optional<DesktopMediaID::Id>
TestPipScreenCaptureCoordinator::GetPipWindowToExcludeFromScreenCapture(
    DesktopMediaID::Id desktop_id) {
  return std::nullopt;
}

std::unique_ptr<PipScreenCaptureCoordinatorProxy>
TestPipScreenCaptureCoordinator::CreateProxy() {
  return nullptr;
}

void TestPipScreenCaptureCoordinator::AddExclusionObserver(
    desktop_capture::PipScreenCaptureExclusionObserver* observer) {
  observers_.AddObserver(observer);
}

void TestPipScreenCaptureCoordinator::RemoveExclusionObserver(
    desktop_capture::PipScreenCaptureExclusionObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestPipScreenCaptureCoordinator::IsExcludedFromScreenCapture() const {
  return is_excluded_;
}

void TestPipScreenCaptureCoordinator::SetExcluded(bool is_excluded) {
  is_excluded_ = is_excluded;
  for (auto& observer : observers_) {
    observer.OnExcludeFromScreenCaptureChanged(is_excluded_);
  }
}

}  // namespace content
