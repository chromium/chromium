// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_PIP_SCREEN_CAPTURE_COORDINATOR_H_
#define CONTENT_TEST_TEST_PIP_SCREEN_CAPTURE_COORDINATOR_H_

#include "base/observer_list.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/public/browser/desktop_capture_pip_utils.h"

namespace content {

// A simple implementation of PipScreenCaptureCoordinator for testing.
class TestPipScreenCaptureCoordinator : public PipScreenCaptureCoordinator {
 public:
  explicit TestPipScreenCaptureCoordinator(bool is_excluded);
  ~TestPipScreenCaptureCoordinator() override;

  // PipScreenCaptureCoordinator:
  void OnPipShown(
      WebContents& pip_web_contents,
      const GlobalRenderFrameHostId& pip_owner_render_frame_host_id) override {}
  void OnPipClosed() override {}
  std::optional<DesktopMediaID::Id> GetPipWindowToExcludeFromScreenCapture(
      DesktopMediaID::Id desktop_id) override;
  std::unique_ptr<PipScreenCaptureCoordinatorProxy> CreateProxy() override;
  void AddExclusionObserver(
      desktop_capture::PipScreenCaptureExclusionObserver* observer) override;
  void RemoveExclusionObserver(
      desktop_capture::PipScreenCaptureExclusionObserver* observer) override;
  bool IsExcludedFromScreenCapture() const override;

  void SetExcluded(bool is_excluded);

 private:
  bool is_excluded_;
  base::ObserverList<desktop_capture::PipScreenCaptureExclusionObserver>
      observers_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_PIP_SCREEN_CAPTURE_COORDINATOR_H_
