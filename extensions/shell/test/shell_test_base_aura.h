// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_TEST_BASE_AURA_H_
#define EXTENSIONS_SHELL_TEST_SHELL_TEST_BASE_AURA_H_

#include <memory>

#include "extensions/browser/extensions_test.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
class AppWindow;
class ShellTestHelperAura;

class ShellTestBaseAura : public ExtensionsTest {
 public:
  ShellTestBaseAura();

  ShellTestBaseAura(const ShellTestBaseAura&) = delete;
  ShellTestBaseAura& operator=(const ShellTestBaseAura&) = delete;

  ~ShellTestBaseAura() override;

  // ExtensionsTest:
  void SetUp() override;
  void TearDown() override;

  // Initializes |app_window| for testing.
  void InitAppWindow(AppWindow* app_window, const gfx::Rect& bounds = {});

 private:
  std::unique_ptr<ShellTestHelperAura> helper_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_TEST_BASE_AURA_H_
