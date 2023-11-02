// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_RUST_GTEST_INTEROP_TEST_TEST_SUBCLASS_H_
#define TESTING_RUST_GTEST_INTEROP_TEST_TEST_SUBCLASS_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/rust_gtest_interop/rust_gtest_interop.h"

namespace rust_gtest_interop {

// A TestSuite that can be used from #[gtest_suite], and which uses the default
// setup through the `RustTest` class and `RUST_GTEST_TEST_SUITE_FACTORY()`
// macro (which is called in the .cc file).
class TestSubclass : public testing::Test {
 public:
  TestSubclass();

  bool get_true() {
    ++calls_;
    return true;
  }
  bool get_false() {
    ++calls_;
    return false;
  }
  int32_t num_calls() const { return calls_; }

 private:
  int32_t calls_ = 0;
};

// A TestSuite that can be used from #[gtest_suite], and which uses a custom C++
// class, `RunTestFromSetup`, to run the test function, instead of the
// `RustTest` class. The `RUST_CUSTOM_TEMPLATE_TEST_SUITE_FACTORY()` macro
// (which is called in the .cc file) will allow use of this class with the
// `RustTestFromSetup` class, and allow the Rust wrapper to request that macro's
// factory when implementing the rust_gtest_interop::TestSuite trait.
class TestSubclassWithCustomTemplate : public testing::Test {
 public:
  TestSubclassWithCustomTemplate();

  int32_t get_three() {
    ++calls_;
    return 3;
  }
  int32_t get_four() {
    ++calls_;
    return 4;
  }
  int32_t num_calls() const { return calls_; }

 private:
  int32_t calls_ = 0;
};

// It's possible to not be able to use the RustTest class from
// rust_gtest_interop. In that case a different macro with a different factory
// function name should be used to provide some type safety.
#define RUST_CUSTOM_TEMPLATE_TEST_SUITE_FACTORY(T)                            \
  extern "C" T* RunTestFromSetupTestFactory_##T(void (*f)(T*)) {              \
    return ::rust_gtest_interop::run_test_from_setup_factory_for_subclass<T>( \
        f);                                                                   \
  }

// This class runs the test from SetUp instead of from TestBody, so the RustTest
// class can't be used with it.
template <class Subclass>
class RunTestFromSetup : public Subclass {
 public:
  explicit RunTestFromSetup(void (&test_fn)(Subclass*)) : test_fn_(test_fn) {
    static_assert(std::is_convertible_v<Subclass*, testing::Test*>,
                  "The Subclass parameter must be a testing::Test or a "
                  "subclass of it");
  }

  void SetUp() override { test_fn_(this); }

 private:
  void TestBody() override {}

  void (&test_fn_)(Subclass*);
};

// Factory method called via `RUST_CUSTOM_TEMPLATE_TEST_SUITE_FACTORY()`.
template <class Subclass>
Subclass* run_test_from_setup_factory_for_subclass(void (*body)(Subclass*)) {
  return new RunTestFromSetup<Subclass>(*body);
}

}  // namespace rust_gtest_interop

#endif  // TESTING_RUST_GTEST_INTEROP_TEST_TEST_SUBCLASS_H_
