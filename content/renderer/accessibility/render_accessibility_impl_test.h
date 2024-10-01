// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_TEST_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_TEST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/public/test/render_view_test.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace content {

using blink::WebAXObject;

class RenderAccessibilityImpl;
class RenderFrameImpl;

class RenderAccessibilityImplTest : public RenderViewTest {
 public:
  RenderAccessibilityImplTest();
  RenderAccessibilityImplTest(const RenderAccessibilityImplTest&) = delete;
  RenderAccessibilityImplTest& operator=(const RenderAccessibilityImplTest&) =
      delete;
  ~RenderAccessibilityImplTest() override = default;

 protected:
  RenderFrameImpl* frame();
  RenderAccessibilityImpl* GetRenderAccessibilityImpl();
  void MarkSubtreeDirty(const WebAXObject& obj);

  // Loads a page given an HTML snippet and initializes its accessibility tree.
  //
  // Consolidates the initialization code required by all tests into a single
  // method.
  void LoadHTMLAndRefreshAccessibilityTree(const char* html);

  void SetUp() override;
  void TearDown() override;

  void SetMode(ui::AXMode mode);
  ui::AXTreeUpdate GetLastAccUpdate();
  const std::vector<ui::AXTreeUpdate>& GetHandledAccUpdates();
  void ClearHandledUpdates();

  std::vector<ui::AXLocationChange>& GetLocationChanges();

  int CountAccessibilityNodesSentToBrowser();

  // RenderFrameImpl::SendPendingAccessibilityEvents() is a protected method, so
  // we wrap it here and access it from tests via this friend class for testing.
  void SendPendingAccessibilityEvents();
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_TEST_H_
