// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl_auralinux.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content::internal {
namespace {

TEST(BrowserAccessibilityStateImplAuraLinuxTest, FindsOrcaSetWithSetproctitle) {
  const char cmdline[] = "orca";
  EXPECT_TRUE(IsOrcaProcess(std::string_view(cmdline, sizeof(cmdline) - 1),
                            /*comm=*/""));
}

TEST(BrowserAccessibilityStateImplAuraLinuxTest,
     FindsOrcaSetWithSetproctitleAndArguments) {
  const char cmdline[] = "orca\0--replace";
  EXPECT_TRUE(IsOrcaProcess(std::string_view(cmdline, sizeof(cmdline) - 1),
                            /*comm=*/""));
}

TEST(BrowserAccessibilityStateImplAuraLinuxTest, FindsOrcaSetWithPrctl) {
  const char cmdline[] = "/usr/bin/python3\0/usr/local/bin/orca\0--replace";
  EXPECT_TRUE(
      IsOrcaProcess(std::string_view(cmdline, sizeof(cmdline) - 1), "orca"));
}

TEST(BrowserAccessibilityStateImplAuraLinuxTest, DoesNotFindUnrelatedProcess) {
  const char cmdline[] = "/usr/bin/python3\0some_script.py";
  EXPECT_FALSE(IsOrcaProcess(std::string_view(cmdline, sizeof(cmdline) - 1),
                             "not-orca"));
}

}  // namespace
}  // namespace content::internal
