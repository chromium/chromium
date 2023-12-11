// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines macros for defining browser tests. The macros mirror
// googletest's built-in `TEST_F()` and `TEST_P()` macros. Note that there is no
// equivalent to googletest's `TEST()` macro, as browser tests always require a
// test fixture.
//
// `IN_PROC_BROWSER_TEST_F(TestFixture, Name)` is analogous to `TEST_F()`.
// `TestFixture` should be a subclass of `content::BrowserTestBase`, e.g.
// `content::ContentBrowserTest` for browser tests that only need features from
// content_shell, `InProcessBrowserTest` for tests that require Chrome features,
// et cetera.
//
// To add additional functionality, e.g. helper methods, to a test fixture,
// subclass `content::ContentBrowserTest`, `InProcessBrowserTest`, et cetera,
// *not* `content::BrowserTestBase`. `content::BrowserTestBase` provides
// low-level functionality shared amongst different types of browser tests, but
// does not actually define how to launch and run the test in an actual browser.
//
// `IN_PROC_BROWSER_TEST_P(TestFixture, Name)` is similar to `TEST_P()`.
// Parameterized tests will need to define their own test fixture by subclassing
// from `content::ContentBrowserTest`, `InProcessBrowserTest`, et cetera as
// noted above *and* `::testing::WithParamInterface<T>`.
//
// Finally, note that like googletest, the browser test fixture is *not* reused
// across different test runs, and each test run gets a clean profile. However,
// browser tests that need to exercise functionality across browser restarts can
// define two tests and link them together using `PRE_` tests, e.g.:
//
//   using MyBrowserTest = content::ContentBrowserTest;
//
//   IN_PROC_BROWSER_TEST_F(MyBrowserTest, PRE_MyTest) {
//     ...
//   }
//
//   IN_PROC_BROWSER_TEST_F(MyBrowserTest, MyTest) {
//     ...
//   }
//
// MyBrowserTest.PRE_MyTest will always run before MyBrowserTest.MyTest; more
// importantly, profile data is not reset between runs. In addition, the `PRE_`
// prefix applies recursively, e.g. `PRE_PRE_MyTest` runs before `PRE_MyTest`.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_H_

// We only want to use InProcessBrowserTest in test targets which properly
// isolate each test case by running each test in a separate process.
// This way if a test hangs the test launcher can reliably terminate it.
//
// InProcessBrowserTest cannot be run more than once in the same address space
// anyway - otherwise the second test crashes.
#if !defined(HAS_OUT_OF_PROC_TEST_RUNNER)
#error Can't reliably terminate hanging event tests without OOP test runner.
#endif

#include "testing/gtest/include/gtest/gtest.h"

#define IN_PROC_BROWSER_TEST_(test_case_name, test_name, parent_class,        \
                              parent_id)                                      \
  class GTEST_TEST_CLASS_NAME_(test_case_name, test_name)                     \
      : public parent_class {                                                 \
   public:                                                                    \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name)() {}                    \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name)                         \
    (const GTEST_TEST_CLASS_NAME_(test_case_name, test_name) &) = delete;     \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name) & operator=(            \
        const GTEST_TEST_CLASS_NAME_(test_case_name, test_name) &) = delete;  \
                                                                              \
   protected:                                                                 \
    void RunTestOnMainThread() override;                                      \
                                                                              \
   private:                                                                   \
    void TestBody() override {}                                               \
    static ::testing::TestInfo* const test_info_;                             \
  };                                                                          \
                                                                              \
  ::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_case_name,           \
                                                    test_name)::test_info_ =  \
      ::testing::internal::MakeAndRegisterTestInfo(                           \
          #test_case_name, #test_name, "", "",                                \
          ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id), \
          parent_class::SetUpTestSuite, parent_class::TearDownTestSuite,      \
          new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(    \
              test_case_name, test_name)>);                                   \
  void GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::RunTestOnMainThread()

#define IN_PROC_BROWSER_TEST_F(test_fixture, test_name)\
  IN_PROC_BROWSER_TEST_(test_fixture, test_name, test_fixture,\
                    ::testing::internal::GetTypeId<test_fixture>())

#define IN_PROC_BROWSER_TEST_P_(test_case_name, test_name)                     \
  class GTEST_TEST_CLASS_NAME_(test_case_name, test_name)                      \
      : public test_case_name {                                                \
   public:                                                                     \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name)() {}                     \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name)                          \
    (const GTEST_TEST_CLASS_NAME_(test_case_name, test_name) &) = delete;      \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name) & operator=(             \
        const GTEST_TEST_CLASS_NAME_(test_case_name, test_name) &) = delete;   \
                                                                               \
   protected:                                                                  \
    void RunTestOnMainThread() override;                                       \
                                                                               \
   private:                                                                    \
    void TestBody() override {}                                                \
    static int AddToRegistry() {                                               \
      ::testing::UnitTest::GetInstance()                                       \
          ->parameterized_test_registry()                                      \
          .GetTestSuitePatternHolder<test_case_name>(                          \
              #test_case_name,                                                 \
              ::testing::internal::CodeLocation(__FILE__, __LINE__))           \
          ->AddTestPattern(                                                    \
              #test_case_name, #test_name,                                     \
              new ::testing::internal::TestMetaFactory<GTEST_TEST_CLASS_NAME_( \
                  test_case_name, test_name)>(),                               \
              ::testing::internal::CodeLocation(__FILE__, __LINE__));          \
      return 0;                                                                \
    }                                                                          \
    static int gtest_registering_dummy_;                                       \
  };                                                                           \
  int GTEST_TEST_CLASS_NAME_(test_case_name,                                   \
                             test_name)::gtest_registering_dummy_ =            \
      GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::AddToRegistry();      \
  void GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::RunTestOnMainThread()

// Wrap the real macro with an outer macro to ensure that the parameters are
// evaluated (e.g., if |test_name| is prefixed with MAYBE_).
#define IN_PROC_BROWSER_TEST_P(test_case_name, test_name) \
  IN_PROC_BROWSER_TEST_P_(test_case_name, test_name)

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_H_
