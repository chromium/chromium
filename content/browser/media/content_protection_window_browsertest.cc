// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/content_protection_window.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace content {

class ContentProtectionWindowTest : public ContentBrowserTest {
 public:
  RenderFrameHost* GetPrimaryMainFrame() {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

  // Helper that creates a `ContentProtectionWindow` and asserts that
  // creation succeeded.
  std::unique_ptr<ContentProtectionWindow> CreateTestWindow() {
    ContentProtectionWindowOrStatus result =
        ContentProtectionWindow::Create(GetPrimaryMainFrame());
    EXPECT_TRUE(result.has_value())
        << "Create() failed with status " << static_cast<int>(result.error());
    return std::move(result).value_or(nullptr);
  }
};

IN_PROC_BROWSER_TEST_F(ContentProtectionWindowTest, CreateReturnsValidWindow) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  ContentProtectionWindowOrStatus result =
      ContentProtectionWindow::Create(GetPrimaryMainFrame());
  ASSERT_TRUE(result.has_value())
      << "Create() failed with status " << static_cast<int>(result.error());
  EXPECT_NE(nullptr, result.value());
  EXPECT_NE(nullptr, result.value()->hwnd());
}

IN_PROC_BROWSER_TEST_F(ContentProtectionWindowTest,
                       HwndIsChildOfBrowserWindow) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  auto window = CreateTestWindow();
  ASSERT_NE(nullptr, window->hwnd());

  HWND parent = ::GetParent(window->hwnd());
  EXPECT_NE(nullptr, parent);

  // The parent should be the frame's top-level browser HWND.
  gfx::NativeView native_view =
      GetPrimaryMainFrame()->GetView()->GetNativeView();
  HWND expected_parent = native_view->GetHost()->GetAcceleratedWidget();
  EXPECT_EQ(expected_parent, parent);
}

IN_PROC_BROWSER_TEST_F(ContentProtectionWindowTest,
                       HwndMatchesParentClientArea) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  auto window = CreateTestWindow();
  ASSERT_NE(nullptr, window->hwnd());

  HWND parent = ::GetParent(window->hwnd());
  ASSERT_NE(nullptr, parent);

  RECT parent_client = {};
  ::GetClientRect(parent, &parent_client);

  RECT child_rect = {};
  ::GetWindowRect(window->hwnd(), &child_rect);

  // Convert child screen coords to parent client coords for comparison.
  POINT child_origin = {child_rect.left, child_rect.top};
  ::ScreenToClient(parent, &child_origin);
  int child_width = child_rect.right - child_rect.left;
  int child_height = child_rect.bottom - child_rect.top;

  EXPECT_EQ(0, child_origin.x);
  EXPECT_EQ(0, child_origin.y);
  EXPECT_EQ(parent_client.right, child_width);
  EXPECT_EQ(parent_client.bottom, child_height);
}

IN_PROC_BROWSER_TEST_F(ContentProtectionWindowTest, ResizesWithParent) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  auto window = CreateTestWindow();
  ASSERT_NE(nullptr, window->hwnd());

  // Resize the shell's window to a size different from its current size
  // to ensure the child HWND actually updates.
  HWND parent = ::GetParent(window->hwnd());
  ASSERT_NE(nullptr, parent);

  RECT original_rect = {};
  ::GetWindowRect(parent, &original_rect);
  int new_width = (original_rect.right - original_rect.left) + 100;
  int new_height = (original_rect.bottom - original_rect.top) + 100;
  ::SetWindowPos(parent, nullptr, 0, 0, new_width, new_height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

  RECT parent_client = {};
  ::GetClientRect(parent, &parent_client);

  RECT child_rect = {};
  ::GetWindowRect(window->hwnd(), &child_rect);
  int child_width = child_rect.right - child_rect.left;
  int child_height = child_rect.bottom - child_rect.top;

  EXPECT_EQ(parent_client.right, child_width);
  EXPECT_EQ(parent_client.bottom, child_height);
}

IN_PROC_BROWSER_TEST_F(ContentProtectionWindowTest,
                       HwndDestroyedOnWindowDestroying) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  auto window = CreateTestWindow();
  ASSERT_NE(nullptr, window->hwnd());

  HWND hwnd_before = window->hwnd();
  EXPECT_TRUE(::IsWindow(hwnd_before));

  // Closing the shell triggers OnWindowDestroying on the aura window,
  // which should destroy the HWND.
  shell()->Close();

  EXPECT_EQ(nullptr, window->hwnd());
  EXPECT_FALSE(::IsWindow(hwnd_before));
}

}  // namespace content
