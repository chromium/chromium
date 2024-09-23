// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/test/ios/wait_util.h"
#import "ios/web/common/crw_input_view_provider.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "ios/web_view/internal/browser_state_keyed_service_factories.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/test/test_with_locale_and_resources.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

class CWVWebViewTest : public TestWithLocaleAndResources {
 public:
  void SetUp() override {
    TestWithLocaleAndResources::SetUp();
    CWVWebView.customUserAgent = nil;

    configuration_ = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::make_unique<WebViewBrowserState>(
                                 /*off_the_record=*/false)];
  }

  void TearDown() override {
    // The BrowserState owned by `configuration_` cannot outlive the
    // `task_environment_` or it will DCHECK.
    [configuration_ shutDown];
    configuration_ = nil;

    CWVWebView.customUserAgent = nil;
    TestWithLocaleAndResources::TearDown();
  }

  CWVWebView* CreateWebView() {
    CGRect frame = CGRectMake(0, 0, 1, 1);
    CWVWebView* web_view = [[CWVWebView alloc] initWithFrame:frame
                                               configuration:configuration_];

    // The WKWebView must be present in the view hierarchy in order to prevent
    // WebKit optimizations which may pause internal parts of the web view
    // without notice. Work around this by adding the view directly.
    UIViewController* view_controller = [GetAnyKeyWindow() rootViewController];
    [view_controller.view addSubview:web_view];

    return web_view;
  }

 protected:
  CWVWebViewTest() : web_client_(std::make_unique<web::WebClient>()) {
    EnsureBrowserStateKeyedServiceFactoriesBuilt();
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  CWVWebViewConfiguration* configuration_;
};

// Test +[CWVWebView customUserAgent].
TEST_F(CWVWebViewTest, CustomUserAgent) {
  EXPECT_FALSE(CWVWebView.customUserAgent);
  CWVWebView.customUserAgent = @"FooCustomUserAgent";
  EXPECT_NSEQ(@"FooCustomUserAgent", CWVWebView.customUserAgent);
}

// Test CWVWebView's inputAccessoryView controls whether or not the overriding
// behavior is enabled.
TEST_F(CWVWebViewTest, InputAccessoryView) {
  CWVWebView* web_view = CreateWebView();

  EXPECT_FALSE(web_view.inputAccessoryView);
  EXPECT_FALSE([web_view webStateInputViewProvider:nil]);

  UIView* input_accessory_view = [[UIView alloc] initWithFrame:web_view.frame];
  web_view.inputAccessoryView = input_accessory_view;
  EXPECT_EQ(web_view, [web_view webStateInputViewProvider:nil]);
}

// Tests CWVWebView's session serialization logic by saving and then
// restoring the WebView state (using only legacy code).
TEST_F(CWVWebViewTest, EncodeDecodeLegacy) {
  // Force the use of legacy storage.
  CWVWebView.useOptimizedSessionStorage = NO;

  NSData* serialized_state = nil;
  {
    CWVWebView* web_view = CreateWebView();

    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]));
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));
    ASSERT_TRUE([web_view canGoBack]);

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);

    [web_view shutDown];
  }

  {
    // Create a new CWVWebView to restore the state into. Check that there is
    // no navigation history in it and then restore the state.
    CWVWebView* web_view = CreateWebView();
    ASSERT_FALSE([web_view canGoBack]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                    error:nil];
    unarchiver.requiresSecureCoding = NO;

    // Verify that the state can been restored correctly.
    [web_view decodeRestorableStateWithCoder:unarchiver];
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return !web_view.loading;
    }));

    // Verify that the state has been restored correctly.
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"],
                web_view.lastCommittedURL);
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"], web_view.visibleURL);
    EXPECT_TRUE([web_view canGoBack]);

    [web_view shutDown];
  }
}

// Tests CWVWebView's session serialization logic by saving and then
// restoring the WebView state (using only optimized code).
TEST_F(CWVWebViewTest, EncodeDecodeOptimized) {
  // Force the use of optimized storage.
  CWVWebView.useOptimizedSessionStorage = YES;

  NSData* serialized_state = nil;
  {
    CWVWebView* web_view = CreateWebView();

    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]));
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));
    ASSERT_TRUE([web_view canGoBack]);

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);

    [web_view shutDown];
  }

  {
    // Create a new CWVWebView to restore the state into. Check that there is
    // no navigation history in it and then restore the state.
    CWVWebView* web_view = CreateWebView();
    ASSERT_FALSE([web_view canGoBack]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                    error:nil];
    unarchiver.requiresSecureCoding = NO;

    // Verify that the state can been restored correctly.
    [web_view decodeRestorableStateWithCoder:unarchiver];
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return !web_view.loading;
    }));

    // Verify that the state has been restored correctly.
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"],
                web_view.lastCommittedURL);
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"], web_view.visibleURL);
    EXPECT_TRUE([web_view canGoBack]);

    [web_view shutDown];
  }
}

// Tests CWVWebView's session serialization logic by saving and then
// restoring the WebView state (migrate legacy to optimised).
TEST_F(CWVWebViewTest, EncodeDecodeMigrateLegacyToOptimized) {
  NSData* serialized_state = nil;
  {
    // Force the use of legacy storage.
    CWVWebView.useOptimizedSessionStorage = NO;
    CWVWebView* web_view = CreateWebView();

    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]));
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));
    ASSERT_TRUE([web_view canGoBack]);

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);

    [web_view shutDown];
  }

  {
    // Force the use of optimized storage.
    CWVWebView.useOptimizedSessionStorage = YES;

    // Create a new CWVWebView to restore the state into. Check that there is
    // no navigation history in it and then restore the state.
    CWVWebView* web_view = CreateWebView();
    ASSERT_FALSE([web_view canGoBack]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                    error:nil];
    unarchiver.requiresSecureCoding = NO;

    // Verify that the state can been restored correctly.
    [web_view decodeRestorableStateWithCoder:unarchiver];
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return !web_view.loading;
    }));

    // Verify that the state has been restored correctly.
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"],
                web_view.lastCommittedURL);
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"], web_view.visibleURL);
    EXPECT_TRUE([web_view canGoBack]);

    [web_view shutDown];
  }
}

// Tests CWVWebView's session serialization logic by saving and then
// restoring the WebView state (migrate optimised to legacy).
TEST_F(CWVWebViewTest, EncodeDecodeMigrateOptimizedToLegacy) {
  NSData* serialized_state = nil;
  {
    // Force the use of optimized storage.
    CWVWebView.useOptimizedSessionStorage = YES;
    CWVWebView* web_view = CreateWebView();

    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]));
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));
    ASSERT_TRUE([web_view canGoBack]);

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);

    [web_view shutDown];
  }

  {
    // Force the use of legacy storage.
    CWVWebView.useOptimizedSessionStorage = NO;

    // Create a new CWVWebView to restore the state into. Check that there is
    // no navigation history in it and then restore the state.
    CWVWebView* web_view = CreateWebView();
    ASSERT_FALSE([web_view canGoBack]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                    error:nil];
    unarchiver.requiresSecureCoding = NO;

    // Verify that the state can been restored correctly.
    [web_view decodeRestorableStateWithCoder:unarchiver];
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return !web_view.loading;
    }));

    // Verify that the state has been restored correctly.
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"],
                web_view.lastCommittedURL);
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"], web_view.visibleURL);
    EXPECT_TRUE([web_view canGoBack]);

    [web_view shutDown];
  }
}

// Tests CWVWebView's session serialization logic by saving and then
// restoring the WebView state (using only legacy code, after shutdown).
TEST_F(CWVWebViewTest, EncodeDecodeLegacyAfterShutdown) {
  // Force the use of legacy storage.
  CWVWebView.useOptimizedSessionStorage = NO;

  NSData* serialized_state = nil;
  {
    CWVWebView* web_view = CreateWebView();

    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]));
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));
    ASSERT_TRUE([web_view canGoBack]);

    // Shutdown before serialization.
    [web_view shutDown];

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);
  }

  {
    // Create a new CWVWebView to restore the state into. Check that there is
    // no navigation history in it and then restore the state.
    CWVWebView* web_view = CreateWebView();
    ASSERT_FALSE([web_view canGoBack]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                    error:nil];
    unarchiver.requiresSecureCoding = NO;

    // Verify that the state can been restored correctly.
    [web_view decodeRestorableStateWithCoder:unarchiver];
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return !web_view.loading;
    }));

    // Verify that the state has been restored correctly.
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"],
                web_view.lastCommittedURL);
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"], web_view.visibleURL);
    EXPECT_TRUE([web_view canGoBack]);

    [web_view shutDown];
  }
}

// Tests CWVWebView's session serialization logic by saving and then
// restoring the WebView state (using only optimized code, after shutdown).
TEST_F(CWVWebViewTest, EncodeDecodeOptimizedAfterShutdown) {
  // Force the use of optimized storage.
  CWVWebView.useOptimizedSessionStorage = YES;

  NSData* serialized_state = nil;
  {
    CWVWebView* web_view = CreateWebView();

    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:newtab"]));
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));
    ASSERT_TRUE([web_view canGoBack]);

    // Shutdown before serialization.
    [web_view shutDown];

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);
  }

  {
    // Create a new CWVWebView to restore the state into. Check that there is
    // no navigation history in it and then restore the state.
    CWVWebView* web_view = CreateWebView();
    ASSERT_FALSE([web_view canGoBack]);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                    error:nil];
    unarchiver.requiresSecureCoding = NO;

    // Verify that the state can been restored correctly.
    [web_view decodeRestorableStateWithCoder:unarchiver];
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return !web_view.loading;
    }));

    // Verify that the state has been restored correctly.
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"],
                web_view.lastCommittedURL);
    EXPECT_NSEQ([NSURL URLWithString:@"about:blank"], web_view.visibleURL);
    EXPECT_TRUE([web_view canGoBack]);

    [web_view shutDown];
  }
}

}  // namespace ios_web_view
