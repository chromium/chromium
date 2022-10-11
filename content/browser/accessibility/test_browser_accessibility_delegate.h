// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_TEST_BROWSER_ACCESSIBILITY_DELEGATE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_TEST_BROWSER_ACCESSIBILITY_DELEGATE_H_

#include "content/browser/accessibility/web_ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"

namespace content {

// TODO(nektar): Rename this class to `TestWebAXPlatformTreeManagerDelegate`.
class TestBrowserAccessibilityDelegate
    : public WebAXPlatformTreeManagerDelegate {
 public:
  TestBrowserAccessibilityDelegate();

  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() override;
  void AccessibilityViewSetFocus() override;
  gfx::Rect AccessibilityGetViewBounds() override;
  float AccessibilityGetDeviceScaleFactor() override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> opt_callback) override;
  bool AccessibilityIsRootFrame() const override;
  WebContentsAccessibility* AccessibilityGetWebContentsAccessibility() override;
  RenderFrameHostImpl* AccessibilityRenderFrameHost() override;

  bool is_root_frame_;
  gfx::AcceleratedWidget accelerated_widget_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_TEST_BROWSER_ACCESSIBILITY_DELEGATE_H_
