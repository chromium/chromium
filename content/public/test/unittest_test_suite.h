// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_UNITTEST_TEST_SUITE_H_
#define CONTENT_PUBLIC_TEST_UNITTEST_TEST_SUITE_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "mojo/core/embedder/configuration.h"
#include "v8/include/v8-forward.h"

namespace base {
class TestSuite;
}

namespace content {
class ContentBrowserClient;
class ContentClient;
class ContentUtilityClient;
class TestBlinkWebUnitTestSupport;
class TestHostResolver;

// A special test suite that also initializes Content its dependencies once for
// all unittests. This is useful for two reasons:
// 1. It ensures runtime dependencies of code in Content and its dependencies
//    are initialized and that using them does not crash. It allows the use of
//    some primitive Blink data types like WebString.
// 2. Individual unittests should not be initializing Blink on their own,
//    initializing it here ensures attempts to do so within an individual test
//     will fail.
class UnitTestTestSuite {
 public:
  // Returned by the unit test binary using this class. ContentClient needs to
  // be set but the others are optional, depending on what tests need in the
  // binary.
  struct ContentClients {
    ContentClients();
    ~ContentClients();

    // Must outlive `content_client`.
    std::unique_ptr<ContentBrowserClient> content_browser_client;
    std::unique_ptr<ContentUtilityClient> content_utility_client;
    std::unique_ptr<ContentClient> content_client;
  };

  // Create test versions of ContentClient interfaces.
  static std::unique_ptr<UnitTestTestSuite::ContentClients>
  CreateTestContentClients();

  // Constructs a new UnitTestTestSuite to wrap `test_suite`.
  // `child_mojo_config`, if provided, is the Mojo configuration to use in any
  // child process spawned by these tests.
  UnitTestTestSuite(
      base::TestSuite* test_suite,
      base::RepeatingCallback<std::unique_ptr<ContentClients>()> create_clients,
      std::optional<mojo::core::Configuration> child_mojo_config =
          std::nullopt);

  UnitTestTestSuite(const UnitTestTestSuite&) = delete;
  UnitTestTestSuite& operator=(const UnitTestTestSuite&) = delete;

  ~UnitTestTestSuite();

  int Run();

  static v8::Isolate* MainThreadIsolateForUnitTestSuite();

 private:
  class UnitTestEventListener;
  UnitTestEventListener* CreateTestEventListener();
  void OnFirstTestStartComplete();

  std::unique_ptr<base::TestSuite> test_suite_;

  std::unique_ptr<TestBlinkWebUnitTestSupport> blink_test_support_;

  raw_ptr<v8::Isolate> isolate_;

  std::unique_ptr<TestHostResolver> test_host_resolver_;

  base::RepeatingCallback<std::unique_ptr<ContentClients>()> create_clients_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_UNITTEST_TEST_SUITE_H_
