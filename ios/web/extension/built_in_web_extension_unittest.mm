// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/extension/built_in_web_extension.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

namespace {

// Returns the path to the dummy extension directory.
base::FilePath GetTestExtensionPath() {
  base::FilePath test_extension_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_extension_path);
  return test_extension_path.AppendASCII("ios/web/test/data/dummy_extension");
}

// Test implementation of `BuiltInWebExtension` returning a configured URL.
class API_AVAILABLE(ios(18.4)) TestBuiltInWebExtension
    : public BuiltInWebExtension {
 public:
  explicit TestBuiltInWebExtension(NSURL* url) : url_(url) {}

  NSURL* GetExtensionURL() const override { return url_; }

 private:
  NSURL* url_ = nil;
};

}  // namespace

class BuiltInWebExtensionTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests loading a valid built-in web extension from test data.
TEST_F(BuiltInWebExtensionTest, TestLoadExtensionSuccess)
API_AVAILABLE(ios(18.4)) {
  NSURL* url = base::apple::FilePathToNSURL(GetTestExtensionPath());
  TestBuiltInWebExtension extension(url);
  EXPECT_NSEQ(url, extension.GetExtensionURL());
  EXPECT_EQ(nil, extension.GetWKWebExtension());

  base::test::TestFuture<WKWebExtension*> future;
  extension.CreateWKWebExtension(future.GetCallback());
  WKWebExtension* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_EQ(result, extension.GetWKWebExtension());

  // Calling CreateWKWebExtension again returns the cached instance.
  base::test::TestFuture<WKWebExtension*> cached_future;
  extension.CreateWKWebExtension(cached_future.GetCallback());
  EXPECT_EQ(result, cached_future.Get());
}

// Tests that concurrent calls to CreateWKWebExtension both receive the loaded
// extension.
TEST_F(BuiltInWebExtensionTest, TestLoadExtensionConcurrentCalls)
API_AVAILABLE(ios(18.4)) {
  NSURL* url = base::apple::FilePathToNSURL(GetTestExtensionPath());
  TestBuiltInWebExtension extension(url);

  base::test::TestFuture<WKWebExtension*> future1;
  base::test::TestFuture<WKWebExtension*> future2;
  extension.CreateWKWebExtension(future1.GetCallback());
  extension.CreateWKWebExtension(future2.GetCallback());

  WKWebExtension* result1 = future1.Get();
  WKWebExtension* result2 = future2.Get();
  ASSERT_NE(nil, result1);
  EXPECT_EQ(result1, result2);
  EXPECT_EQ(result1, extension.GetWKWebExtension());
}

// Tests loading a non-existent extension directory.
TEST_F(BuiltInWebExtensionTest, TestLoadExtensionFailure)
API_AVAILABLE(ios(18.4)) {
  NSURL* invalid_url = [NSURL fileURLWithPath:@"/non/existent/path"
                                  isDirectory:YES];
  TestBuiltInWebExtension extension(invalid_url);
  EXPECT_EQ(nil, extension.GetWKWebExtension());

  base::test::TestFuture<WKWebExtension*> future;
  extension.CreateWKWebExtension(future.GetCallback());
  WKWebExtension* result = future.Get();
  EXPECT_EQ(nil, result);
  EXPECT_EQ(nil, extension.GetWKWebExtension());

  // Calling CreateWKWebExtension again returns nil immediately.
  base::test::TestFuture<WKWebExtension*> failed_future;
  extension.CreateWKWebExtension(failed_future.GetCallback());
  EXPECT_EQ(nil, failed_future.Get());
}

}  // namespace web
