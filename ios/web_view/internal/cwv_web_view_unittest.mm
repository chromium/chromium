// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
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
#import "ios/web_view/public/cwv_back_forward_list.h"
#import "ios/web_view/public/cwv_back_forward_list_item.h"
#import "ios/web_view/test/test_with_locale_and_resources.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {

// Returns the first WKWebView found in the view hierarchy of `view`.
WKWebView* FindWKWebView(UIView* view) {
  if (WKWebView* web_view = base::apple::ObjCCast<WKWebView>(view)) {
    return web_view;
  }
  for (UIView* subview in view.subviews) {
    WKWebView* web_view = FindWKWebView(subview);
    if (web_view) {
      return web_view;
    }
  }
  return nil;
}

}  // namespace

class CWVWebViewTest : public TestWithLocaleAndResources {
 public:
  void SetUp() override {
    TestWithLocaleAndResources::SetUp();

    configuration_ = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::make_unique<WebViewBrowserState>(
                                 /*off_the_record=*/false)];
  }

  void TearDown() override {
    // The BrowserState owned by `configuration_` cannot outlive the
    // `task_environment_` or it will DCHECK.
    [configuration_ shutDown];
    configuration_ = nil;

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

// Tests that -prewarm does not add a back/forward list item, so the first page
// loaded afterwards is the first item.
TEST_F(CWVWebViewTest, Prewarm) {
  CWVWebView* web_view = CreateWebView();

  [web_view prewarm];
  EXPECT_FALSE(web_view.canGoBack);
  EXPECT_FALSE(web_view.canGoForward);
  EXPECT_FALSE(web_view.loading);
  EXPECT_FALSE(web_view.visibleURL);
  EXPECT_FALSE(web_view.backForwardList.currentItem);
  EXPECT_EQ(0U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);

  NSURL* url = [NSURL URLWithString:@"about:blank"];
  ASSERT_TRUE(test::LoadUrl(web_view, url));
  EXPECT_FALSE(web_view.canGoBack);
  EXPECT_NSEQ(url, web_view.backForwardList.currentItem.URL);
  EXPECT_EQ(0U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);
}

// Tests that calling -prewarm more than once is safe.
TEST_F(CWVWebViewTest, PrewarmTwice) {
  CWVWebView* web_view = CreateWebView();

  [web_view prewarm];
  [web_view prewarm];
  EXPECT_FALSE(web_view.backForwardList.currentItem);

  NSURL* url = [NSURL URLWithString:@"about:blank"];
  ASSERT_TRUE(test::LoadUrl(web_view, url));
  EXPECT_FALSE(web_view.canGoBack);
  EXPECT_NSEQ(url, web_view.backForwardList.currentItem.URL);
  EXPECT_EQ(0U, web_view.backForwardList.backList.count);
}

// Tests that -prewarm does nothing after a page has been loaded.
TEST_F(CWVWebViewTest, PrewarmAfterLoad) {
  CWVWebView* web_view = CreateWebView();

  NSURL* url = [NSURL URLWithString:@"about:newtab"];
  ASSERT_TRUE(test::LoadUrl(web_view, url));
  CWVBackForwardListItem* item = web_view.backForwardList.currentItem;
  ASSERT_TRUE(item);

  [web_view prewarm];
  EXPECT_FALSE(web_view.loading);
  EXPECT_FALSE(web_view.canGoBack);
  EXPECT_NSEQ(item, web_view.backForwardList.currentItem);
  EXPECT_NSEQ(url, web_view.lastCommittedURL);
  EXPECT_EQ(0U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);
}

// Tests -prewarm with a non persistent (incognito) configuration: the first
// page loaded is the only back/forward list item, and the prewarmed WKWebView
// uses a non persistent data store.
TEST_F(CWVWebViewTest, PrewarmNonPersistent) {
  CWVWebViewConfiguration* configuration = [[CWVWebViewConfiguration alloc]
      initWithBrowserState:std::make_unique<WebViewBrowserState>(
                               /*off_the_record=*/true,
                               configuration_.browserState)];
  ASSERT_FALSE(configuration.persistent);
  CWVWebView* web_view =
      [[CWVWebView alloc] initWithFrame:CGRectMake(0, 0, 1, 1)
                          configuration:configuration];
  [[GetAnyKeyWindow() rootViewController].view addSubview:web_view];

  [web_view prewarm];
  EXPECT_FALSE(web_view.backForwardList.currentItem);

  NSURL* url = [NSURL URLWithString:@"about:blank"];
  ASSERT_TRUE(test::LoadUrl(web_view, url));
  EXPECT_FALSE(web_view.canGoBack);
  EXPECT_NSEQ(url, web_view.backForwardList.currentItem.URL);
  EXPECT_EQ(0U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);

  WKWebView* wk_web_view = FindWKWebView(web_view);
  ASSERT_TRUE(wk_web_view);
  EXPECT_FALSE(wk_web_view.configuration.websiteDataStore.persistent);

  [web_view removeFromSuperview];
  [configuration shutDown];
}

// Tests that the restorable state of a prewarmed web view that never loaded
// anything has no history: restoring it gives an empty web view, whose first
// load is the only back/forward list item.
TEST_F(CWVWebViewTest, PrewarmEncodeDecode) {
  NSData* serialized_state = nil;
  {
    CWVWebView* web_view = CreateWebView();
    [web_view prewarm];

    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [web_view encodeRestorableStateWithCoder:archiver];
    serialized_state = [archiver encodedData];
    ASSERT_TRUE(serialized_state);

    [web_view shutDown];
  }

  CWVWebView* web_view = CreateWebView();
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                  error:nil];
  unarchiver.requiresSecureCoding = NO;
  [web_view decodeRestorableStateWithCoder:unarchiver];
  EXPECT_FALSE(web_view.backForwardList.currentItem);
  EXPECT_FALSE(web_view.canGoBack);

  NSURL* url = [NSURL URLWithString:@"about:blank"];
  ASSERT_TRUE(test::LoadUrl(web_view, url));
  EXPECT_FALSE(web_view.canGoBack);
  EXPECT_NSEQ(url, web_view.backForwardList.currentItem.URL);
  EXPECT_EQ(0U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);
}

// Tests that state can be restored into a prewarmed web view (the restored
// session replaces it, with no extra back/forward list item), and that -prewarm
// does nothing on the restored web view.
TEST_F(CWVWebViewTest, PrewarmThenDecode) {
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

  CWVWebView* web_view = CreateWebView();
  [web_view prewarm];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:serialized_state
                                                  error:nil];
  unarchiver.requiresSecureCoding = NO;
  [web_view decodeRestorableStateWithCoder:unarchiver];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web_view.loading;
  }));

  NSURL* url = [NSURL URLWithString:@"about:blank"];
  EXPECT_NSEQ(url, web_view.lastCommittedURL);
  EXPECT_TRUE(web_view.canGoBack);
  EXPECT_EQ(1U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);

  [web_view prewarm];
  EXPECT_FALSE(web_view.loading);
  EXPECT_NSEQ(url, web_view.lastCommittedURL);
  EXPECT_EQ(1U, web_view.backForwardList.backList.count);
  EXPECT_EQ(0U, web_view.backForwardList.forwardList.count);

  [web_view shutDown];
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

// Tests that setting obscuredContentInsets on CWVWebView stores and returns the
// value, and applies it to the underlying proxy/controller on iOS 26+.
TEST_F(CWVWebViewTest, ObscuredContentInsets) {
  if (@available(iOS 26.0, *)) {
    CWVWebView* web_view = CreateWebView();
    ASSERT_TRUE(test::LoadUrl(web_view, [NSURL URLWithString:@"about:blank"]));

    UIEdgeInsets insets = UIEdgeInsetsMake(10, 20, 30, 40);
    ASSERT_FALSE(
        UIEdgeInsetsEqualToEdgeInsets(insets, web_view.obscuredContentInsets));
    web_view.obscuredContentInsets = insets;

    EXPECT_TRUE(
        UIEdgeInsetsEqualToEdgeInsets(insets, web_view.obscuredContentInsets));

    [web_view shutDown];
  }
}

}  // namespace ios_web_view
