// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/model/screenshot_delegate.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/stringprintf.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

// A fake object that implements BrowserProvider and BrowserProviderInterface.
@interface ScreenshotDelegateTestProvider
    : NSObject <BrowserProviderInterface, BrowserProvider>
@end

@implementation ScreenshotDelegateTestProvider {
  std::unique_ptr<TestBrowser> _browser;
}

- (void)dealloc {
  CHECK(!_browser) << "-shutdown must be called before dealloc";
}

- (void)shutdown {
  _browser.reset();
}

- (void)createBrowserWithProfile:(ProfileIOS*)profile {
  CHECK(!profile->IsOffTheRecord());
  _browser = std::make_unique<TestBrowser>(profile);
}

#pragma mark - BrowserProviderInterface

- (id<BrowserProvider>)mainBrowserProvider {
  return self;
}

- (id<BrowserProvider>)currentBrowserProvider {
  return self;
}

- (id<BrowserProvider>)incognitoBrowserProvider {
  return nil;
}

- (BOOL)hasIncognitoBrowserProvider {
  return NO;
}

#pragma mark - BrowserProvider

- (Browser*)browser {
  return _browser.get();
}

- (UIViewController*)viewController:(BrowserProviderPassKey)key {
  NOTREACHED();
}

@end

class ScreenshotDelegateTest : public PlatformTest {
 protected:
  ScreenshotDelegateTest() { profile_ = TestProfileIOS::Builder().Build(); }
  ~ScreenshotDelegateTest() override {}

  void SetUp() override {
    PlatformTest::SetUp();
    browser_provider_interface_ = [[ScreenshotDelegateTestProvider alloc] init];
    screenshot_service_ = OCMClassMock([UIScreenshotService class]);
  }

  void TearDown() override {
    [browser_provider_interface_ shutdown];
    browser_provider_interface_ = nil;
    PlatformTest::TearDown();
  }

  void CreateScreenshotDelegate() {
    screenshot_delegate_ = [[ScreenshotDelegate alloc]
        initWithBrowserProviderInterface:browser_provider_interface_];
  }

  // Creates a `FakeWebState` and sets its browser state and current URL.
  std::unique_ptr<web::FakeWebState> CreateFakeWebState(const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(url);
    return web_state;
  }

  // Sets up a browser with a single tab containing `web_state` and configures
  // the screenshot delegate to use it.
  void SetupMockTab(std::unique_ptr<web::FakeWebState> web_state) {
    [browser_provider_interface_ createBrowserWithProfile:profile_.get()];
    Browser* browser = browser_provider_interface_.mainBrowserProvider.browser;
    WebStateList* web_state_list = browser->GetWebStateList();

    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));

    CreateScreenshotDelegate();
  }

  // Sets up a Data Controls policy to block screenshots for the given `url`.
  void SetScreenshotBlockingPolicy(const GURL& url) {
    data_controls::SetDataControls(
        profile_->GetPrefs(),
        {base::StringPrintf(R"({
                                "sources": { "urls": ["%s"] },
                                "restrictions": [
                                  { "class": "SCREENSHOT", "level": "BLOCK" }
                                ]
                              })",
                            url.spec().c_str())});
  }

  // Verifies that calling
  // `screenshotService:generatePDFRepresentationWithCompletion:` behaves as
  // expected regarding blocking.
  void VerifyScreenshotBlocked(bool expected_blocked) {
    __block bool callback_ran = false;
    [screenshot_delegate_ screenshotService:screenshot_service_
        generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                  NSInteger indexOfCurrentPage,
                                                  CGRect rectInCurrentPage) {
          EXPECT_EQ(PDFData == nil, expected_blocked);
          callback_ran = true;
        }];
    EXPECT_TRUE(callback_ran);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ScreenshotDelegateTestProvider* browser_provider_interface_;
  ScreenshotDelegate* screenshot_delegate_;
  id screenshot_service_;
};

// Tests that ScreenshotDelegate can be init with browserProviderInterface can
// be set and that data can be generated from it.
TEST_F(ScreenshotDelegateTest, ScreenshotService) {
  // Expected: Empty NSData.
  auto web_state = std::make_unique<web::FakeWebState>();

  CRWWebViewScrollViewProxy* scroll_view_proxy =
      [[CRWWebViewScrollViewProxy alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [scroll_view_proxy setScrollView:scroll_view];
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
  web_state->SetWebViewProxy(web_view_proxy_mock);

  // Fake scroll_view contentOffset, contentSize and frame.
  CGPoint content_offset = CGPointMake(10.0, 15.0);
  CGSize content_size = CGSizeMake(425, 4000);
  CGRect frame = CGRectMake(0, 0, 375, 812);
  scroll_view.contentOffset = content_offset;
  scroll_view.contentSize = content_size;
  scroll_view.frame = frame;

  CGRect expected_rect_in_page = CGRectZero;

  expected_rect_in_page =
      CGRectMake(content_offset.x,
                 content_size.height - frame.size.height - content_offset.y,
                 frame.size.width, frame.size.height);

  SetupMockTab(std::move(web_state));

  __block int nbCalls = 0;
  [screenshot_delegate_ screenshotService:screenshot_service_
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        EXPECT_TRUE(PDFData);
        EXPECT_TRUE(
            CGRectEqualToRect(expected_rect_in_page, rectInCurrentPage));
        ++nbCalls;
      }];

  EXPECT_EQ(1, nbCalls);
}

// Tests that when ScreenshotDelegate's browserProviderInterface has a nil
// Browser screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilBrowser) {
  // Expected: nil NSData.

  // Assert that the BrowserProviderIterface has no Browser.
  ASSERT_FALSE(browser_provider_interface_.mainBrowserProvider.browser);

  CreateScreenshotDelegate();
  VerifyScreenshotBlocked(true);
}

// Tests that when ScreenshotDelegate's browserProviderInterface has a nil
// WebSatate screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilWebState) {
  // Expected: nil NSData.

  // Create an empty Browser in the BrowserProviderInterface.
  [browser_provider_interface_ createBrowserWithProfile:profile_.get()];
  ASSERT_TRUE(browser_provider_interface_.mainBrowserProvider.browser);

  CreateScreenshotDelegate();
  VerifyScreenshotBlocked(true);
}

// Tests that when DataProtectionTabHelper has screenshot protection enabled,
// screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, ScreenshotBlockedByDataProtection) {
  GURL url("https://protected.com");
  auto web_state = CreateFakeWebState(url);

  SetScreenshotBlockingPolicy(url);

  DataProtectionTabHelper::CreateForWebState(web_state.get());
  ASSERT_TRUE(DataProtectionTabHelper::FromWebState(web_state.get())
                  ->IsScreenshotProtectionEnabled());

  SetupMockTab(std::move(web_state));

  VerifyScreenshotBlocked(true);
}

// Tests that when DataProtectionTabHelper has screenshot protection disabled,
// screenshotService will return data.
TEST_F(ScreenshotDelegateTest, ScreenshotNotBlockedByDataProtection) {
  auto web_state = CreateFakeWebState(GURL("https://unprotected.com"));

  DataProtectionTabHelper::CreateForWebState(web_state.get());
  ASSERT_FALSE(DataProtectionTabHelper::FromWebState(web_state.get())
                   ->IsScreenshotProtectionEnabled());

  SetupMockTab(std::move(web_state));

  VerifyScreenshotBlocked(false);
}

