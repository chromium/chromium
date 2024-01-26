// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_shell_platform_delegate.h"

#include "base/command_line.h"
#include "content/public/browser/web_contents.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "content/web_test/browser/web_test_javascript_dialog_manager.h"
#include "content/web_test/common/web_test_switches.h"

namespace content {

// This file contains platform-independent web test overrides of the
// ShellPlatformDelegate class. Platform-dependent code is found in the various
// platform-suffixed implementation files.

// static
bool WebTestShellPlatformDelegate::IsHeadless() {
  // Headless by default in web tests, unless overridden on the command line.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableHeadlessMode);
}

void WebTestShellPlatformDelegate::DidCreateOrAttachWebContents(
    Shell* shell,
    WebContents* web_contents) {
  WebTestControlHost::Get()->DidCreateOrAttachWebContents(web_contents);
}

void WebTestShellPlatformDelegate::DidCloseLastWindow() {
  // Some tests, or some fuzzer's test cases are closing every window. When
  // this happens, the test runner must run the next test. For this reason, this
  // do not call Shell::Shutdown(). It will be called manually at the end of:
  // WebTestBrowserMainRunner::RunBrowserMain().
}

std::unique_ptr<JavaScriptDialogManager>
WebTestShellPlatformDelegate::CreateJavaScriptDialogManager(Shell* shell) {
  return std::make_unique<WebTestJavaScriptDialogManager>();
}

bool WebTestShellPlatformDelegate::HandlePointerLockRequest(
    Shell* shell,
    WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  if (!user_gesture && !last_unlocked_by_target) {
    web_contents->GotResponseToPointerLockRequest(
        blink::mojom::PointerLockResult::kRequiresUserGesture);
  }

  WebTestControlHost::Get()->RequestPointerLock(web_contents);
  // Always indicate that we have handled the request to lock the mouse.
  return true;
}

bool WebTestShellPlatformDelegate::ShouldAllowRunningInsecureContent(
    Shell* shell) {
  WebTestControlHost* control_host = WebTestControlHost::Get();
  return control_host->web_test_runtime_flags()
      .running_insecure_content_allowed();
}

}  // namespace content
