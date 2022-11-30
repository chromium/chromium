// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_shell_platform_delegate.h"

#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_platform_data_aura.h"

namespace content {

struct WebTestShellPlatformDelegate::WebTestShellData {};
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
  if (!IsHeadless())
    GetShellPlatformDataAura()->ShowWindow();
  // Shared implementation with headless mode, except for the ShowWindow().
  ShellPlatformDelegate::CreatePlatformWindow(shell, initial_size);
}

gfx::NativeWindow WebTestShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  return ShellPlatformDelegate::GetNativeWindow(shell);
}

void WebTestShellPlatformDelegate::CleanUp(Shell* shell) {
  ShellPlatformDelegate::CleanUp(shell);
}

void WebTestShellPlatformDelegate::SetContents(Shell* shell) {
  ShellPlatformDelegate::SetContents(shell);
}

void WebTestShellPlatformDelegate::EnableUIControl(Shell* shell,
                                                   UIControl control,
                                                   bool is_enabled) {
  ShellPlatformDelegate::EnableUIControl(shell, control, is_enabled);
}

void WebTestShellPlatformDelegate::SetAddressBarURL(Shell* shell,
                                                    const GURL& url) {
  ShellPlatformDelegate::SetAddressBarURL(shell, url);
}

void WebTestShellPlatformDelegate::SetTitle(Shell* shell,
                                            const std::u16string& title) {
  ShellPlatformDelegate::SetTitle(shell, title);
}

void WebTestShellPlatformDelegate::MainFrameCreated(Shell* shell) {
  ShellPlatformDelegate::MainFrameCreated(shell);
}

bool WebTestShellPlatformDelegate::DestroyShell(Shell* shell) {
  return ShellPlatformDelegate::DestroyShell(shell);
}

void WebTestShellPlatformDelegate::ResizeWebContent(
    Shell* shell,
    const gfx::Size& content_size) {
  ShellPlatformDelegate::ResizeWebContent(shell, content_size);
}

}  // namespace content
