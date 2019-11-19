// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_TEST_SHELL_TEST_HELPER_AURA_H_
#define EXTENSIONS_SHELL_BROWSER_TEST_SHELL_TEST_HELPER_AURA_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
namespace test {
class AuraTestHelper;
}
}  // namespace aura

namespace ui {
class TestContextFactories;
}

namespace extensions {

class AppWindow;

// A helper class that does common Aura initialization required for the shell.
class ShellTestHelperAura {
 public:
  ShellTestHelperAura();
  ~ShellTestHelperAura();

  // Initializes common test dependencies.
  void SetUp();

  // Cleans up.
  void TearDown();

  // Initializes |app_window| for testing.
  void InitAppWindow(AppWindow* app_window, const gfx::Rect& bounds = {});

 private:
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<aura::test::AuraTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestHelperAura);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_TEST_SHELL_TEST_HELPER_AURA_H_
