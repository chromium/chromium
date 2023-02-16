// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_base_aura.h"

#include "extensions/shell/test/shell_test_extensions_browser_client.h"
#include "extensions/shell/test/shell_test_helper_aura.h"

namespace extensions {

ShellTestBaseAura::ShellTestBaseAura() = default;

ShellTestBaseAura::~ShellTestBaseAura() = default;

void ShellTestBaseAura::SetUp() {
  helper_ = std::make_unique<ShellTestHelperAura>();
  helper_->SetUp();
  std::unique_ptr<TestExtensionsBrowserClient> extensions_browser_client =
      std::make_unique<ShellTestExtensionsBrowserClient>();
  SetExtensionsBrowserClient(std::move(extensions_browser_client));
  ExtensionsTest::SetUp();
}

void ShellTestBaseAura::TearDown() {
  ExtensionsTest::TearDown();
  helper_->TearDown();
}

void ShellTestBaseAura::InitAppWindow(AppWindow* app_window,
                                      const gfx::Rect& bounds) {
  helper_->InitAppWindow(app_window, bounds);
}

}  // namespace extensions
