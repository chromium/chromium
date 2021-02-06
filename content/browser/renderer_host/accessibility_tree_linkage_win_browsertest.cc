// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/aura/client/aura_constants.h"

namespace content {

struct AccessibilityLinkageTestParams {
  bool is_uia_enabled;
  bool is_legacy_window_disabled;
} const kTestParameters[] = {{false, false},
                             {false, true},
                             {true, false},
                             {true, true}};

class AccessibilityTreeLinkageWinBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<AccessibilityLinkageTestParams> {
 public:
  AccessibilityTreeLinkageWinBrowserTest() {
    dummy_ax_platform_node_ = ui::AXPlatformNode::Create(&dummy_ax_node_);
  }

  ~AccessibilityTreeLinkageWinBrowserTest() override {
    dummy_ax_platform_node_->Destroy();
    dummy_ax_platform_node_ = nullptr;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().is_uia_enabled)
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ::switches::kEnableExperimentalUIAutomation);
    if (GetParam().is_legacy_window_disabled)
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ::switches::kDisableLegacyIntermediateWindow);
  }

  RenderWidgetHostViewAura* GetView() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  gfx::NativeWindow GetParentWindow() { return GetView()->window()->parent(); }

  LegacyRenderWidgetHostHWND* GetLegacyRenderWidgetHostHWND() {
    return GetView()->legacy_render_widget_host_HWND_;
  }

 protected:
  ui::AXPlatformNodeDelegateBase dummy_ax_node_;
  ui::AXPlatformNode* dummy_ax_platform_node_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityTreeLinkageWinBrowserTest);
};

IN_PROC_BROWSER_TEST_P(AccessibilityTreeLinkageWinBrowserTest, Linkage) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  GetParentWindow()->SetProperty(
      aura::client::kParentNativeViewAccessibleKey,
      dummy_ax_platform_node_->GetNativeViewAccessible());

  if (GetParam().is_legacy_window_disabled)
    ASSERT_EQ(GetLegacyRenderWidgetHostHWND(), nullptr);
  else
    ASSERT_NE(GetLegacyRenderWidgetHostHWND(), nullptr);

  // Used by WebView to splice in the web content root accessible as a child of
  // the WebView's parent
  gfx::NativeViewAccessible native_view_accessible =
      GetView()->GetNativeViewAccessible();
  EXPECT_EQ(native_view_accessible, GetView()
                                        ->host()
                                        ->GetRootBrowserAccessibilityManager()
                                        ->GetRoot()
                                        ->GetNativeViewAccessible());

  // Used by LegacyRenderWidgetHostHWND to find the parent of the UIA fragment
  // root for web content
  gfx::NativeViewAccessible parent_native_view_accessible =
      GetView()->GetParentNativeViewAccessible();
  EXPECT_EQ(parent_native_view_accessible,
            dummy_ax_platform_node_->GetNativeViewAccessible());

  // Used by BrowserAccessibilityManager to find the parent of the web content
  // root accessible
  gfx::NativeViewAccessible accessibility_native_view_accessible =
      GetView()->AccessibilityGetNativeViewAccessible();
  if (GetParam().is_legacy_window_disabled) {
    EXPECT_EQ(accessibility_native_view_accessible,
              dummy_ax_platform_node_->GetNativeViewAccessible());
  } else {
    EXPECT_EQ(accessibility_native_view_accessible,
              GetLegacyRenderWidgetHostHWND()->window_accessible());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AccessibilityTreeLinkageWinBrowserTest,
                         testing::ValuesIn(kTestParameters));

}  // namespace content
