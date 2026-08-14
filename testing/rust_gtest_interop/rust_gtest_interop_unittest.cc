// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/543717800): Re-enable after fixing small_ctor support for
// Fuchsia.
// TODO(crbug.com/462501862): Rust gtest interop is disabled on Mac/iOS ASan.
#if BUILDFLAG(IS_FUCHSIA) || (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_APPLE))
#define MAYBE_VerifyTestsRegistered DISABLED_VerifyTestsRegistered
#else
#define MAYBE_VerifyTestsRegistered VerifyTestsRegistered
#endif
TEST(RustGTestInteropTest, MAYBE_VerifyTestsRegistered) {
  auto* unit_test = testing::UnitTest::GetInstance();
  int rust_test_count = 0;
  int exact_suite_test_count = 0;
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const auto* test_suite = unit_test->GetTestSuite(i);
    auto test_suite_name = std::string_view(test_suite->name());
    if (test_suite_name == "RustGTestInteropTest") {
      rust_test_count = test_suite->total_test_count();
    }
    if (test_suite_name == "RustGTestInteropTestExactSuite") {
      exact_suite_test_count = test_suite->total_test_count();
    }
  }
  // We expect 11 tests in RustGTestInteropTest (including the DISABLED one).
  EXPECT_EQ(rust_test_count, 11);
  EXPECT_EQ(exact_suite_test_count, 1);
}
