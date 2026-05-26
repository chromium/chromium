// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_MOUSE_CURSOR_OVERLAY_CONTROLLER_UNITTEST_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_MOUSE_CURSOR_OVERLAY_CONTROLLER_UNITTEST_H_

#include "base/test/scoped_feature_list.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

class WebContents;

class MockOverlay final : public MouseCursorOverlayController::Overlay {
 public:
  MockOverlay();
  ~MockOverlay() override;

  void SetImageAndBounds(const SkBitmap& image,
                         const gfx::RectF& bounds) final {}
  void SetBounds(const gfx::RectF& bounds) final {}
  MOCK_METHOD(void, OnCapturedMouseEvent, (const gfx::Point&), (override));
};

class MouseCursorOverlayControllerTestBase : public RenderViewHostTestHarness {
 public:
  MouseCursorOverlayControllerTestBase();
  ~MouseCursorOverlayControllerTestBase() override;

  void RunRestrictsToWebContentsTest();

 protected:
  virtual void SetupCaptureTarget(WebContents* target_web_contents,
                                  const gfx::Rect& bounds) = 0;
  virtual gfx::NativeView GetTargetView() = 0;
  virtual void InitializeEventGenerator() = 0;
  virtual void SendMouseMove(const gfx::Point& position_in_parent) = 0;
  virtual gfx::Point GetExpectedCapturedPosition(
      const gfx::Point& position_in_parent,
      const gfx::Rect& target_bounds) = 0;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_MOUSE_CURSOR_OVERLAY_CONTROLLER_UNITTEST_H_
