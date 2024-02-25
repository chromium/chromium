// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/rust_gtest_interop/rust_gtest_interop.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace rust_gtest_interop {

extern "C" testing::Test* rust_gtest_default_factory(
    void (*body)(testing::Test*)) {
  return rust_gtest_factory_for_subclass<testing::Test>(body);
}

extern "C" void rust_gtest_add_test(GtestFactoryFunction gtest_factory,
                                    void (*test_function)(testing::Test*),
                                    const char* test_suite_name,
                                    const char* test_name,
                                    const char* file,
                                    int32_t line) {
  auto factory = [=]() { return gtest_factory(test_function); };
  testing::RegisterTest(test_suite_name, test_name, nullptr, nullptr, file,
                        line, factory);
}

extern "C" void rust_gtest_add_failure_at(const char* file,
                                          int32_t line,
                                          const char* message) {
  ADD_FAILURE_AT(reinterpret_cast<const char*>(file), line) << message;
}

}  // namespace rust_gtest_interop
