// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_shell_platform_delegate.h"

#include "base/command_line.h"
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

std::unique_ptr<JavaScriptDialogManager>
WebTestShellPlatformDelegate::CreateJavaScriptDialogManager(Shell* shell) {
  return std::make_unique<WebTestJavaScriptDialogManager>();
}

std::unique_ptr<BluetoothChooser>
WebTestShellPlatformDelegate::RunBluetoothChooser(
    Shell* shell,
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  return WebTestControlHost::Get()->RunBluetoothChooser(frame, event_handler);
}

bool WebTestShellPlatformDelegate::ShouldAllowRunningInsecureContent(
    Shell* shell) {
  const base::DictionaryValue& flags =
      WebTestControlHost::Get()->accumulated_web_test_runtime_flags_changes();
  return flags.FindBoolPath("running_insecure_content_allowed").value_or(false);
}

}  // namespace content
