// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_FUZZTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_BROWSER_FUZZTEST_SUPPORT_H_

#include <concepts>

#include "base/test/scoped_run_loop_timeout.h"
#include "content/public/test/browser_test_base.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

template <typename T>
  requires(std::derived_from<T, content::BrowserTestBase>)
class BrowserFuzzTest : public T, public fuzztest::FuzzTestRunnerFixture {
  using Base = T;

 public:
  BrowserFuzzTest() = default;
  void RunTestOnMainThread() override { std::move(run_fuzz_test_)(); }
  void FuzzTestRunner(absl::AnyInvocable<void() &&> run_test) override {
    run_fuzz_test_ = std::move(run_test);
    SetUp();
    TearDown();
  }
  void SetUp() override {
    // Overrides the default 60s run loop timeout set by `BrowserTestBase`. See
    // https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/browser_test_base.cc?q=ScopedRunLoopTimeout.
    // All of the fuzzing engines that we use are having timeouts features, and
    // this timeout can vary depending on the number of tested testcases. We
    // must let the engines handle timeouts, and set the maximum here.
    base::test::ScopedRunLoopTimeout scoped_timeout(FROM_HERE,
                                                    base::TimeDelta::Max());
    Base::SetUp();
  }
  void TearDown() override { Base::TearDown(); }
  void TestBody() override {}

 private:
  absl::AnyInvocable<void() &&> run_fuzz_test_;
};

#endif  // CONTENT_PUBLIC_TEST_BROWSER_FUZZTEST_SUPPORT_H_
