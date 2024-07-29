// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BLINK_TEST_ENVIRONMENT_H_
#define CONTENT_PUBLIC_TEST_BLINK_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-forward.h"

// This package provides functions used by blink_unittests.
namespace content {

class TestBlinkWebUnitTestSupport;

// Sets up Blink test environment for all unit tests of a test suiteß.
// This can be installed as a gtest Environment:
//
// testing::Environment* const blink_env =
//   testing::AddGlobalTestEnvironment(new BlinkTestEnvironment);
//
// TODO(crbug.com/40221845): Move this to blink/renderer/controllers/tests/
class BlinkTestEnvironment : public ::testing::Environment {
 public:
  BlinkTestEnvironment();
  ~BlinkTestEnvironment() override;
  BlinkTestEnvironment(const BlinkTestEnvironment&) = delete;
  BlinkTestEnvironment& operator=(const BlinkTestEnvironment&) = delete;

  void SetUp() override;
  void TearDown() override;

 protected:
  virtual void InitializeBlinkTestSupport();

  base::test::ScopedFeatureList scoped_feature_list_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
  std::optional<content::TestContentClientInitializer> content_initializer_;
  std::unique_ptr<content::TestBlinkWebUnitTestSupport> blink_test_support_;
};

// Similar to BlinkTestEnvironment, but additionally initializes a Blink Main
// Thread Isolate. Prefer using BlinkTestEnvironment for the test suite and
// manually initialize a scope isolate in tests that need it instead.
class BlinkTestEnvironmentWithIsolate : public BlinkTestEnvironment {
 public:
  v8::Isolate* GetMainThreadIsolate() { return isolate_.get(); }

  void TearDown() override;

 protected:
  void InitializeBlinkTestSupport() override;

 private:
  raw_ptr<v8::Isolate> isolate_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BLINK_TEST_ENVIRONMENT_H_
