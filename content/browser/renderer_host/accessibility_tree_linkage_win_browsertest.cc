// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_features.h"
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
    if (GetParam().is_uia_enabled) {
      scoped_feature_list_.InitAndEnableFeature(::features::kUiaProvider);
    }
    dummy_ax_platform_node_ = ui::AXPlatformNode::Create(&dummy_ax_node_);
  }

  AccessibilityTreeLinkageWinBrowserTest(
      const AccessibilityTreeLinkageWinBrowserTest&) = delete;
  AccessibilityTreeLinkageWinBrowserTest& operator=(
      const AccessibilityTreeLinkageWinBrowserTest&) = delete;

  ~AccessibilityTreeLinkageWinBrowserTest() override {
    dummy_ax_platform_node_->Destroy();
    dummy_ax_platform_node_ = nullptr;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

 protected:
  ui::AXPlatformNodeDelegate dummy_ax_node_;
  raw_ptr<ui::AXPlatformNode, DanglingUntriaged> dummy_ax_platform_node_;
};

IN_PROC_BROWSER_TEST_P(AccessibilityTreeLinkageWinBrowserTest, Linkage) {
  testing::ScopedContentAXModeSetter ax_mode_setter(ui::kAXModeBasic.flags());

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
                                        ->GetBrowserAccessibilityRoot()
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
                         ::testing::ValuesIn(kTestParameters));

}  // namespace content
