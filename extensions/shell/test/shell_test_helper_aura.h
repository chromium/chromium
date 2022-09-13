// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_TEST_HELPER_AURA_H_
#define EXTENSIONS_SHELL_TEST_SHELL_TEST_HELPER_AURA_H_

#include <memory>

#include "ui/gfx/geometry/rect.h"

namespace aura {
namespace test {
class AuraTestHelper;
}
}  // namespace aura

namespace extensions {

class AppWindow;

// A helper class that does common Aura initialization required for the shell.
class ShellTestHelperAura {
 public:
  ShellTestHelperAura();

  ShellTestHelperAura(const ShellTestHelperAura&) = delete;
  ShellTestHelperAura& operator=(const ShellTestHelperAura&) = delete;

  ~ShellTestHelperAura();

  // Initializes common test dependencies.
  void SetUp();

  // Cleans up.
  void TearDown();

  // Initializes |app_window| for testing.
  void InitAppWindow(AppWindow* app_window, const gfx::Rect& bounds = {});

 private:
  std::unique_ptr<aura::test::AuraTestHelper> helper_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_TEST_HELPER_AURA_H_
