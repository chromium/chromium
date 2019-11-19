// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_system_caret_win.h"
#include "ui/base/win/hwnd_subclass.h"

namespace content {

class AccessibilityObjectLifetimeWinBrowserTest
    : public content::ContentBrowserTest {
 public:
  AccessibilityObjectLifetimeWinBrowserTest() = default;
  ~AccessibilityObjectLifetimeWinBrowserTest() override = default;

 protected:
  RenderWidgetHostViewAura* GetView() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  void CacheRootNode() {
    GetView()
        ->legacy_render_widget_host_HWND_->GetOrCreateWindowRootAccessible()
        ->QueryInterface(IID_PPV_ARGS(&test_node_));
  }

  void CacheCaretNode() {
    GetView()
        ->legacy_render_widget_host_HWND_->ax_system_caret_->GetCaret()
        ->QueryInterface(IID_PPV_ARGS(&test_node_));
  }

  HWND GetHwnd() { return GetView()->AccessibilityGetAcceleratedWidget(); }

  Microsoft::WRL::ComPtr<ui::AXPlatformNodeWin> test_node_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityObjectLifetimeWinBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeWinBrowserTest,
                       RootDoesNotLeak) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Cache a pointer to the root node we return to Windows.
  CacheRootNode();

  // Repeatedly call the public API to obtain an accessibility object. If our
  // code is leaking references, this will drive up the reference count.
  for (int i = 0; i < 10; i++) {
    Microsoft::WRL::ComPtr<IAccessible> root_accessible;
    EXPECT_HRESULT_SUCCEEDED(::AccessibleObjectFromWindow(
        GetHwnd(), OBJID_CLIENT, IID_PPV_ARGS(&root_accessible)));
    EXPECT_NE(root_accessible.Get(), nullptr);
  }

  // Close the main window.
  shell()->Close();

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->m_dwRef, 1);
}

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeWinBrowserTest,
                       CaretDoesNotLeak) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Cache a pointer to the object we return to Windows.
  CacheCaretNode();

  // Repeatedly call the public API to obtain an accessibility object. If our
  // code is leaking references, this will drive up the reference count.
  for (int i = 0; i < 10; i++) {
    Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
    EXPECT_HRESULT_SUCCEEDED(::AccessibleObjectFromWindow(
        GetHwnd(), OBJID_CARET, IID_PPV_ARGS(&caret_accessible)));
    EXPECT_NE(caret_accessible.Get(), nullptr);
  }

  // Close the main window.
  shell()->Close();

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->m_dwRef, 1);
}

// Window subclassing message filter for the legacy window to allow us to
// examine state after the call to LegacyRenderWidgetHostHWND::Destroy() but
// before the window is torn down completely. Operating system hooks can run in
// this narrow window; see crbug.com/945584 for one example.
class AccessibilityTeardownTestMessageFilter : public ui::HWNDMessageFilter {
 public:
  AccessibilityTeardownTestMessageFilter(RenderWidgetHostViewAura* view)
      : view_(view) {
    HWND hwnd = view->AccessibilityGetAcceleratedWidget();
    CHECK(hwnd);
    ui::HWNDSubclass::AddFilterToTarget(hwnd, this);
  }
  ~AccessibilityTeardownTestMessageFilter() override = default;

  // ui::HWNDMessageFilter:
  bool FilterMessage(HWND hwnd,
                     UINT message,
                     WPARAM w_param,
                     LPARAM l_param,
                     LRESULT* l_result) override {
    if (message == WM_DESTROY) {
      // Verify that the view no longer exposes a NativeViewAccessible.
      EXPECT_EQ(view_->GetNativeViewAccessible(), nullptr);

      // Remove ourselves as a subclass.
      ui::HWNDSubclass::RemoveFilterFromAllTargets(this);
    }

    return true;
  }

 private:
  RenderWidgetHostViewAura* view_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeWinBrowserTest,
                       DoNotReturnObjectDuringTeardown) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityTeardownTestMessageFilter test_message_filter(GetView());

  shell()->Close();
}

class AccessibilityObjectLifetimeUiaWinBrowserTest
    : public AccessibilityObjectLifetimeWinBrowserTest {
 public:
  AccessibilityObjectLifetimeUiaWinBrowserTest() = default;
  ~AccessibilityObjectLifetimeUiaWinBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalUIAutomation);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityObjectLifetimeUiaWinBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeUiaWinBrowserTest,
                       RootDoesNotLeak) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Cache a pointer to the root node we return to Windows.
  CacheRootNode();

  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  // Repeatedly call the public API to obtain an accessibility object. If our
  // code is leaking references, this will drive up the reference count.
  for (int i = 0; i < 10; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> root_element;
    EXPECT_HRESULT_SUCCEEDED(uia->ElementFromHandle(GetHwnd(), &root_element));
    EXPECT_NE(root_element.Get(), nullptr);

    // Raise an event on the root node. This will cause UIA to cache a pointer
    // to it.
    ::UiaRaiseStructureChangedEvent(
        test_node_.Get(), StructureChangeType_ChildrenInvalidated, nullptr, 0);
  }

  // Close the main window.
  shell()->Close();

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->m_dwRef, 1);
}

}  // namespace content
