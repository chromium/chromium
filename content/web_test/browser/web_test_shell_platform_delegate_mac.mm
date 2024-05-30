// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_shell_platform_delegate.h"

#import "base/apple/foundation_util.h"
#include "base/containers/contains.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/shell/browser/shell.h"

namespace content {

// On mac, the WebTestShellPlatformDelegate replaces behaviour in the base class
// ShellPlatformDelegate when in headless mode. Otherwise it mostly defers to
// the base class.

struct WebTestShellPlatformDelegate::WebTestShellData {
  gfx::Size initial_size;
};

struct WebTestShellPlatformDelegate::WebTestPlatformData {};

WebTestShellPlatformDelegate::WebTestShellPlatformDelegate() = default;
WebTestShellPlatformDelegate::~WebTestShellPlatformDelegate() = default;

void WebTestShellPlatformDelegate::Initialize(
    const gfx::Size& default_window_size) {
  ShellPlatformDelegate::Initialize(default_window_size);
}

void WebTestShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::CreatePlatformWindow(shell, initial_size);
    return;
  }

  DCHECK(!base::Contains(web_test_shell_data_map_, shell));
  WebTestShellData& shell_data = web_test_shell_data_map_[shell];

  shell_data.initial_size = initial_size;
}

gfx::NativeWindow WebTestShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  if (!IsHeadless())
    return ShellPlatformDelegate::GetNativeWindow(shell);

  NOTREACHED_IN_MIGRATION();
  return {};
}

void WebTestShellPlatformDelegate::CleanUp(Shell* shell) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::CleanUp(shell);
    return;
  }

  DCHECK(base::Contains(web_test_shell_data_map_, shell));
  web_test_shell_data_map_.erase(shell);
  if (shell == activated_headless_shell_)
    activated_headless_shell_ = nullptr;
}

void WebTestShellPlatformDelegate::SetContents(Shell* shell) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::SetContents(shell);
    return;
  }
}

void WebTestShellPlatformDelegate::EnableUIControl(Shell* shell,
                                                   UIControl control,
                                                   bool is_enabled) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::EnableUIControl(shell, control, is_enabled);
    return;
  }
}

void WebTestShellPlatformDelegate::SetAddressBarURL(Shell* shell,
                                                    const GURL& url) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::SetAddressBarURL(shell, url);
    return;
  }
}

void WebTestShellPlatformDelegate::SetTitle(Shell* shell,
                                            const std::u16string& title) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::SetTitle(shell, title);
    return;
  }
}

void WebTestShellPlatformDelegate::MainFrameCreated(Shell* shell) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::MainFrameCreated(shell);
    return;
  }

  DCHECK(base::Contains(web_test_shell_data_map_, shell));
  WebTestShellData& shell_data = web_test_shell_data_map_[shell];

  // In mac headless mode, the OS view for the WebContents is not attached to a
  // window so the usual notifications from the OS about the bounds of the web
  // contents do not occur. We need to make sure the renderer knows its bounds,
  // and to do this we force a resize to happen on the WebContents. However, the
  // WebContents can not be fully resized until after the RenderWidgetHostView
  // is created, which may not be not done until the first navigation starts.
  // Failing to do this resize *after* the navigation causes the
  // RenderWidgetHostView to be created would leave the WebContents with invalid
  // sizes (such as the window screen rect).
  //
  // We use the signal that the `blink::WebView` has been created in the
  // renderer as a proxy for knowing when the top level RenderWidgetHostView is
  // created, since they are created at the same time.
  DCHECK(shell->web_contents()->GetPrimaryMainFrame()->GetView());
  ResizeWebContent(shell, shell_data.initial_size);
}

bool WebTestShellPlatformDelegate::DestroyShell(Shell* shell) {
  if (IsHeadless())
    return false;  // Shell destroys itself.
  return ShellPlatformDelegate::DestroyShell(shell);
}

void WebTestShellPlatformDelegate::ResizeWebContent(
    Shell* shell,
    const gfx::Size& content_size) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::ResizeWebContent(shell, content_size);
    return;
  }

  NSView* web_view = shell->web_contents()->GetNativeView().GetNativeNSView();
  web_view.frame =
      NSMakeRect(0, 0, content_size.width(), content_size.height());

  // The above code changes the RenderWidgetHostView's size, but does not change
  // the widget's screen rects, since the RenderWidgetHostView is not attached
  // to a window in headless mode. So this call causes them to be updated so
  // they are not left as 0x0.
  auto* rwhv_mac = shell->web_contents()->GetPrimaryMainFrame()->GetView();
  if (rwhv_mac)
    rwhv_mac->SetWindowFrameInScreen(gfx::Rect(content_size));
}

void WebTestShellPlatformDelegate::ActivateContents(Shell* shell,
                                                    WebContents* top_contents) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::ActivateContents(shell, top_contents);
    return;
  }

  // In headless mode, there are no system windows, so we can't go down the
  // normal path which relies on calling the OS to move focus/active states.
  // Instead we fake it out by just informing the RenderWidgetHost directly.

  // For all windows other than this one, blur them.
  for (Shell* window : Shell::windows()) {
    if (window != shell) {
      WebContents* other_top_contents = window->web_contents();
      auto* other_rwhv_mac = static_cast<RenderWidgetHostViewMac*>(
          other_top_contents->GetPrimaryMainFrame()->GetView());
      other_rwhv_mac->OnFirstResponderChanged(false);
      other_rwhv_mac->GetRenderWidgetHost()->SetActive(false);
    }
  }

  auto* top_rwhv_mac = static_cast<RenderWidgetHostViewMac*>(
      top_contents->GetPrimaryMainFrame()->GetView());
  top_rwhv_mac->OnFirstResponderChanged(true);
  top_rwhv_mac->GetRenderWidgetHost()->SetActive(true);
  activated_headless_shell_ = shell;
}

void WebTestShellPlatformDelegate::DidNavigatePrimaryMainFramePostCommit(
    Shell* shell,
    WebContents* contents) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::DidNavigatePrimaryMainFramePostCommit(shell,
                                                                 contents);
    return;
  }

  // Normally RenderFrameHostManager::CommitPending() transfers focus status to
  // the new RenderWidgetHostView when a navigation creates a new view, but that
  // doesn't work in Mac headless mode because RenderWidgetHostView depends on
  // the native window (which doesn't exist in headless mode) to manage focus
  // status. Instead we manually set focus status of the new RenderWidgetHost.
  if (shell == activated_headless_shell_)
    ActivateContents(shell, contents);
}

bool WebTestShellPlatformDelegate::HandleKeyboardEvent(
    Shell* shell,
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (IsHeadless())
    return false;
  return ShellPlatformDelegate::HandleKeyboardEvent(shell, source, event);
}

}  // namespace content
