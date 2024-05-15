// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_shell_platform_delegate.h"

#include "base/containers/contains.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_platform_data_aura.h"

namespace content {

struct WebTestShellPlatformDelegate::WebTestShellData {
  gfx::Size content_size;
};

struct WebTestShellPlatformDelegate::WebTestPlatformData {
  // Only used in headless mode. Uses `wm::WMState` from the
  // ShellPlatformDelegate, which must outlive this.
  std::unique_ptr<ShellPlatformDataAura> aura;
};

WebTestShellPlatformDelegate::WebTestShellPlatformDelegate() = default;
WebTestShellPlatformDelegate::~WebTestShellPlatformDelegate() = default;

void WebTestShellPlatformDelegate::Initialize(
    const gfx::Size& default_window_size) {
  ShellPlatformDelegate::Initialize(default_window_size);

  if (IsHeadless()) {
    web_test_platform_ = std::make_unique<WebTestPlatformData>();
    web_test_platform_->aura =
        std::make_unique<ShellPlatformDataAura>(default_window_size);
  }
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

  shell_data.content_size = initial_size;

  web_test_platform_->aura->ResizeWindow(initial_size);
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
}

void WebTestShellPlatformDelegate::SetContents(Shell* shell) {
  if (!IsHeadless()) {
    ShellPlatformDelegate::SetContents(shell);
    return;
  }

  DCHECK(base::Contains(web_test_shell_data_map_, shell));
  WebTestShellData& shell_data = web_test_shell_data_map_[shell];

  aura::Window* content = shell->web_contents()->GetNativeView();
  aura::Window* parent = web_test_platform_->aura->host()->window();
  if (!parent->Contains(content)) {
    parent->AddChild(content);
    // Move the cursor to a fixed position before tests run to avoid getting
    // an unpredictable result from mouse events.
    content->MoveCursorTo(gfx::Point());
    content->Show();
  }
  content->SetBounds(gfx::Rect(shell_data.content_size));
  RenderWidgetHostView* host_view =
      shell->web_contents()->GetRenderWidgetHostView();
  if (host_view)
    host_view->SetSize(shell_data.content_size);
}

void WebTestShellPlatformDelegate::EnableUIControl(Shell* shell,
                                                   UIControl control,
                                                   bool is_enabled) {
  if (!IsHeadless())
    ShellPlatformDelegate::EnableUIControl(shell, control, is_enabled);
  // Nothing in headless mode.
}

void WebTestShellPlatformDelegate::SetAddressBarURL(Shell* shell,
                                                    const GURL& url) {
  if (!IsHeadless())
    ShellPlatformDelegate::SetAddressBarURL(shell, url);
  // Nothing in headless mode.
}

void WebTestShellPlatformDelegate::SetTitle(Shell* shell,
                                            const std::u16string& title) {
  if (!IsHeadless())
    ShellPlatformDelegate::SetTitle(shell, title);
  // Nothing in headless mode.
}

void WebTestShellPlatformDelegate::MainFrameCreated(Shell* shell) {
  // No difference in headless mode.
  ShellPlatformDelegate::MainFrameCreated(shell);
}

bool WebTestShellPlatformDelegate::DestroyShell(Shell* shell) {
  if (!IsHeadless())
    return ShellPlatformDelegate::DestroyShell(shell);

  return false;  // Shell destroys itself.
}

void WebTestShellPlatformDelegate::ResizeWebContent(
    Shell* shell,
    const gfx::Size& content_size) {
  // No difference in headless mode.
  ShellPlatformDelegate::ResizeWebContent(shell, content_size);
}

}  // namespace content
