// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/model/screenshot_delegate.h"

#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
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

class ScreenshotDelegateTest : public PlatformTest {
 protected:
  ScreenshotDelegateTest() { profile_ = TestProfileIOS::Builder().Build(); }
  ~ScreenshotDelegateTest() override {}

  void SetUp() override {
    browser_interface_ = [[StubBrowserProvider alloc] init];
    browser_provider_interface_ = [[StubBrowserProviderInterface alloc] init];
    screenshot_service_ = OCMClassMock([UIScreenshotService class]);
  }

  void createScreenshotDelegate() {
    screenshot_delegate_ = [[ScreenshotDelegate alloc]
        initWithBrowserProviderInterface:browser_provider_interface_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  StubBrowserProvider* browser_interface_;
  StubBrowserProviderInterface* browser_provider_interface_;
  ScreenshotDelegate* screenshot_delegate_;
  id screenshot_service_;
};

// Tests that ScreenshotDelegate can be init with browserProviderInterface can
// be set and that data can be generated from it.
TEST_F(ScreenshotDelegateTest, ScreenshotService) {
  // Expected: Empty NSData.
  auto web_state = std::make_unique<web::FakeWebState>();
  TestBrowser browser(profile_.get());

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

  // Insert the web_state into the Browser.
  int insertion_index =
      browser.GetWebStateList()->InsertWebState(std::move(web_state));
  browser.GetWebStateList()->ActivateWebStateAt(insertion_index);

  // Add the Browser to StubBrowserProvider.
  browser_interface_.browser = &browser;

  // Add the StubBrowserProvider to StubBrowserProviderInterface.
  browser_provider_interface_.currentBrowserProvider = browser_interface_;

  createScreenshotDelegate();

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
  // Add the StubBrowserProvider with no set Browser to
  // StubBrowserProviderInterface.
  browser_provider_interface_.currentBrowserProvider = browser_interface_;

  createScreenshotDelegate();

  __block bool callback_ran = false;
  [screenshot_delegate_ screenshotService:screenshot_service_
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        EXPECT_FALSE(PDFData);
        callback_ran = true;
      }];

  EXPECT_TRUE(callback_ran);
}

// Tests that when ScreenshotDelegate's browserProviderInterface has a nil
// WebSatate screenshotService will return nil.
TEST_F(ScreenshotDelegateTest, NilWebState) {
  // Expected: nil NSData.
  TestBrowser browser(profile_.get());

  // Add the empty Browser to StubBrowserProvider.
  browser_interface_.browser = &browser;

  // Add the StubBrowserProvider to StubBrowserProviderInterface.
  browser_provider_interface_.currentBrowserProvider = browser_interface_;

  createScreenshotDelegate();

  __block bool callback_ran = false;
  [screenshot_delegate_ screenshotService:screenshot_service_
      generatePDFRepresentationWithCompletion:^(NSData* PDFData,
                                                NSInteger indexOfCurrentPage,
                                                CGRect rectInCurrentPage) {
        EXPECT_FALSE(PDFData);
        callback_ran = true;
      }];

  EXPECT_TRUE(callback_ran);
}
