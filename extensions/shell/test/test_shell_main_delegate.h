// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_
#define EXTENSIONS_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/shell/app/shell_main_delegate.h"

namespace content {
class ContentUtilityClient;
}

namespace extensions {

class TestShellMainDelegate : public extensions::ShellMainDelegate {
 public:
  TestShellMainDelegate();

  TestShellMainDelegate(const TestShellMainDelegate&) = delete;
  TestShellMainDelegate& operator=(const TestShellMainDelegate&) = delete;

  ~TestShellMainDelegate() override;

 protected:
  // content::ContentMainDelegate implementation:
  content::ContentUtilityClient* CreateContentUtilityClient() override;

 private:
  std::unique_ptr<content::ContentUtilityClient> utility_client_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_
