// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_SHELL_PLATFORM_DELEGATE_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_SHELL_PLATFORM_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/shell/browser/shell_platform_delegate.h"

namespace content {

class WebTestShellPlatformDelegate : public ShellPlatformDelegate {
 public:
  WebTestShellPlatformDelegate();
  ~WebTestShellPlatformDelegate() override;

  // ShellPlatformDelegate overrides.
  void Initialize(const gfx::Size& default_window_size) override;
  void CreatePlatformWindow(Shell* shell,
                            const gfx::Size& initial_size) override;
  void DidCreateOrAttachWebContents(Shell* shell,
                                    WebContents* web_contents) override;
  void DidCloseLastWindow() override;
  gfx::NativeWindow GetNativeWindow(Shell* shell) override;
  void CleanUp(Shell* shell) override;
  void SetContents(Shell* shell) override;
  void EnableUIControl(Shell* shell,
                       UIControl control,
                       bool is_enabled) override;
  void SetAddressBarURL(Shell* shell, const GURL& url) override;
  void SetTitle(Shell* shell, const std::u16string& title) override;
  void MainFrameCreated(Shell* shell) override;
  std::unique_ptr<JavaScriptDialogManager> CreateJavaScriptDialogManager(
      Shell* shell) override;
  bool HandlePointerLockRequest(Shell* shell,
                                WebContents* web_contents,
                                bool user_gesture,
                                bool last_unlocked_by_target) override;
  bool ShouldAllowRunningInsecureContent(Shell* shell) override;
  bool DestroyShell(Shell* shell) override;
  void ResizeWebContent(Shell* shell, const gfx::Size& content_size) override;
#if BUILDFLAG(IS_MAC)
  void ActivateContents(Shell* shell, WebContents* top_contents) override;
  void DidNavigatePrimaryMainFramePostCommit(Shell*,
                                             WebContents* contents) override;
  bool HandleKeyboardEvent(Shell* shell,
                           WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
#endif

 private:
  // When headless, no window is shown while running the tests.
  static bool IsHeadless();

  // Data held for each Shell instance, since there is one ShellPlatformDelegate
  // for the whole browser process (shared across Shells). This is defined for
  // each platform implementation.
  struct WebTestShellData;

  // Holds an instance of WebTestShellData for each Shell.
  base::flat_map<Shell*, WebTestShellData> web_test_shell_data_map_;

  // Data held in ShellPlatformDelegate that is shared between all Shells. This
  // is created in Initialize(), and is defined for each platform
  // implementation.
  struct WebTestPlatformData;
  std::unique_ptr<WebTestPlatformData> web_test_platform_;

#if BUILDFLAG(IS_MAC)
  // The last headless shell that called ActivateContents().
  raw_ptr<Shell> activated_headless_shell_ = nullptr;
#endif
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_SHELL_PLATFORM_DELEGATE_H_
