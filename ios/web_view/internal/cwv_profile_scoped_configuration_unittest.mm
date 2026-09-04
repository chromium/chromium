// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/run_until.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_web_view.h"
#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "ios/web_view/test/test_with_locale_and_resources.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace ios_web_view {

class CWVProfileScopedConfigurationTest : public TestWithLocaleAndResources {
 protected:
  CWVProfileScopedConfigurationTest()
      : web_client_(std::make_unique<web::WebClient>()) {}

  void SetUp() override {
    TestWithLocaleAndResources::SetUp();
    if (!@available(iOS 17.0, *)) {
      GTEST_SKIP() << "Profile-scoped configurations require iOS 17.0+";
    }
  }

  ~CWVProfileScopedConfigurationTest() override {
    [CWVWebViewConfiguration shutDown];
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  web::ScopedTestingWebClient web_client_;
};

// Tests that configurationWithIdentifier: returns the same instance for the
// same identifier, and correctly exposes storageIdentifier.
TEST_F(CWVProfileScopedConfigurationTest, MultitonCaching) {
  if (@available(iOS 17.0, *)) {
    NSUUID* uuid1 = [NSUUID UUID];
    NSUUID* uuid2 = [NSUUID UUID];

    CWVWebViewConfiguration* config1 =
        [CWVWebViewConfiguration configurationWithIdentifier:uuid1];
    CWVWebViewConfiguration* config1_again =
        [CWVWebViewConfiguration configurationWithIdentifier:uuid1];
    CWVWebViewConfiguration* config2 =
        [CWVWebViewConfiguration configurationWithIdentifier:uuid2];

    EXPECT_NSEQ(config1, config1_again);
    EXPECT_NSNE(config1, config2);
    EXPECT_NSEQ(config1.storageIdentifier, uuid1);
    EXPECT_NSEQ(config2.storageIdentifier, uuid2);
    EXPECT_EQ([CWVWebViewConfiguration defaultConfiguration].storageIdentifier,
              nil);
  }
}

// Tests that profile paths are correctly isolated.
TEST_F(CWVProfileScopedConfigurationTest, PathIsolation) {
  if (@available(iOS 17.0, *)) {
    NSUUID* uuid = [NSUUID UUID];
    CWVWebViewConfiguration* config =
        [CWVWebViewConfiguration configurationWithIdentifier:uuid];

    base::FilePath state_path = config.browserState->GetStatePath();

    std::string uuid_string = base::SysNSStringToUTF8(uuid.UUIDString);
    EXPECT_TRUE(state_path.EndsWithSeparator() ||
                state_path.BaseName().value() == uuid_string)
        << "Path " << state_path.value() << " does not end with "
        << uuid_string;
    EXPECT_EQ(state_path.DirName().BaseName().value(), "Profiles");
  }
}

// Tests that remove cleans up the profile directory on disk.
TEST_F(CWVProfileScopedConfigurationTest, Cleanup) {
  if (@available(iOS 17.0, *)) {
    NSUUID* uuid = [NSUUID UUID];
    CWVWebViewConfiguration* config =
        [CWVWebViewConfiguration configurationWithIdentifier:uuid];

    base::FilePath state_path = config.browserState->GetStatePath();

    @autoreleasepool {
      __unused CWVWebView* web_view =
          [[CWVWebView alloc] initWithFrame:CGRectZero configuration:config];
    }

    ASSERT_TRUE(base::DirectoryExists(state_path));

    [config remove];

    EXPECT_TRUE(
        base::test::RunUntil([&] { return config.browserState == nullptr; }));

    // Allow background deletion tasks (e.g. from PrefService or WebDataService)
    // to complete before verifying the directory is gone.
    base::ThreadPoolInstance::Get()->FlushForTesting();

    EXPECT_TRUE(base::test::RunUntil(
        [&] { return !base::DirectoryExists(state_path); }));
  }
}

// Tests that hasActiveWebViews correctly tracks CWVWebView instances.
TEST_F(CWVProfileScopedConfigurationTest, HasActiveWebViews) {
  if (@available(iOS 17.0, *)) {
    NSUUID* uuid = [NSUUID UUID];
    CWVWebViewConfiguration* config =
        [CWVWebViewConfiguration configurationWithIdentifier:uuid];

    EXPECT_FALSE(config.hasActiveWebViews);

    @autoreleasepool {
      CWVWebView* webView = [[CWVWebView alloc] initWithFrame:CGRectZero
                                                configuration:config];
      EXPECT_TRUE(config.hasActiveWebViews);
      [webView shutDown];
    }

    EXPECT_FALSE(config.hasActiveWebViews);
  }
}

}  // namespace ios_web_view
