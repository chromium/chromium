// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_tab_helper.h"

#import <string>

#import "base/containers/circular_deque.h"
#import "base/containers/contains.h"
#import "base/strings/string_split.h"
#import "base/strings/stringprintf.h"
#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using infobars::InfoBarDelegate;

namespace {

const base::circular_deque<std::string>& GetEvents() {
  return breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
}

bool EventsEmpty() {
  return GetEvents().empty();
}

}  // namespace

// Test fixture for BreadcrumbManagerTabHelper class.
class BreadcrumbManagerTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder test_profile_builder;
    profile_ = std::move(test_profile_builder).Build();

    first_web_state_.SetBrowserState(profile_.get());
    second_web_state_.SetBrowserState(profile_.get());

    // Navigation manager is needed for InfobarManager.
    first_web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&first_web_state_);
    second_web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&second_web_state_);

    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    scroll_view_ = [[UIScrollView alloc] init];
    [scroll_view_proxy setScrollView:scroll_view_];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
    first_web_state_.SetWebViewProxy(web_view_proxy_mock);

    BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState first_web_state_;
  web::FakeWebState second_web_state_;
  UIScrollView* scroll_view_ = nil;
};

// Tests that the identifier returned for a WebState is unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueIdentifiers) {
  BreadcrumbManagerTabHelper::CreateForWebState(&second_web_state_);

  int first_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebState(&first_web_state_)
          ->GetUniqueId();
  int second_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebState(&second_web_state_)
          ->GetUniqueId();

  EXPECT_GT(first_tab_identifier, 0);
  EXPECT_GT(second_tab_identifier, 0);
  EXPECT_NE(first_tab_identifier, second_tab_identifier);
}

// Tests that BreadcrumbManagerTabHelper events are logged to the associated
// BreadcrumbManagerKeyedService. This test does not attempt to validate that
// every observer method is correctly called as that is done in the
// WebStateObserverTest tests.
TEST_F(BreadcrumbManagerTabHelperTest, EventsLogged) {
  EXPECT_TRUE(EventsEmpty());
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();

  first_web_state_.OnNavigationFinished(&context);
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(base::Contains(events.back(),
                             breadcrumbs::kBreadcrumbDidFinishNavigation))
      << events.back();
}

// Tests that BreadcrumbManagerTabHelper events logged from seperate WebStates
// are unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueEvents) {
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);

  BreadcrumbManagerTabHelper::CreateForWebState(&second_web_state_);
  second_web_state_.OnNavigationStarted(&context);

  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_STRNE(events.front().c_str(), events.back().c_str());
  EXPECT_TRUE(base::Contains(events.front(),
                             breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.front();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
}

// Tests metadata for www.google.com navigation.
TEST_F(BreadcrumbManagerTabHelperTest, GoogleNavigationStart) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.google.com"));
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(
      base::Contains(events.front(), breadcrumbs::kBreadcrumbGoogleNavigation))
      << events.front();
}

// Tests metadata for https://play.google.com/ navigation.
TEST_F(BreadcrumbManagerTabHelperTest, GooglePlayNavigationStart) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://play.google.com/"));
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  // #google is useful to indicate SRP. There is no need to know URLs of other
  // visited google properties.
  EXPECT_FALSE(
      base::Contains(events.front(), breadcrumbs::kBreadcrumbGoogleNavigation))
      << events.front();
}

// Tests metadata for chrome://newtab NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, ChromeNewTabNavigationStart) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kChromeUINewTabURL));
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(
      events.front(),
      base::StringPrintf("%s%lld", breadcrumbs::kBreadcrumbDidStartNavigation,
                         context.GetNavigationId())))
      << events.front();
  EXPECT_TRUE(
      base::Contains(events.front(), breadcrumbs::kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests metadata for about://newtab/ NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, AboutNewTabNavigationStart) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("about://newtab/"));
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(
      events.front(),
      base::StringPrintf("%s%lld", breadcrumbs::kBreadcrumbDidStartNavigation,
                         context.GetNavigationId())))
      << events.front();
  EXPECT_TRUE(
      base::Contains(events.front(), breadcrumbs::kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests unique ID in DidStartNavigation and DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationUniqueId) {
  ASSERT_TRUE(EventsEmpty());

  // DidStartNavigation
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(
      events.front(),
      base::StringPrintf("%s%lld", breadcrumbs::kBreadcrumbDidStartNavigation,
                         context.GetNavigationId())))
      << events.front();

  // DidFinishNavigation
  first_web_state_.OnNavigationFinished(&context);
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(base::Contains(
      events.back(),
      base::StringPrintf("%s%lld", breadcrumbs::kBreadcrumbDidFinishNavigation,
                         context.GetNavigationId())))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByUser) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(true);
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), "#link")) << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(),
                             breadcrumbs::kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_FALSE(base::Contains(
      events.back(), breadcrumbs::kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByScript) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(false);
  context.SetPageTransition(ui::PAGE_TRANSITION_RELOAD);
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), "#reload")) << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_FALSE(base::Contains(events.back(),
                              breadcrumbs::kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(),
                             breadcrumbs::kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests browser initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, BrowserInitiatedByScript) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(false);
  context.SetPageTransition(ui::PAGE_TRANSITION_TYPED);
  first_web_state_.OnNavigationStarted(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), "#typed")) << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_FALSE(base::Contains(events.back(),
                              breadcrumbs::kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_FALSE(base::Contains(
      events.back(), breadcrumbs::kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests download navigation.
TEST_F(BreadcrumbManagerTabHelperTest, Download) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  context.SetIsDownload(true);
  first_web_state_.OnNavigationFinished(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(),
                             breadcrumbs::kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbDownload))
      << events.back();
}

// Tests PDF load.
TEST_F(BreadcrumbManagerTabHelperTest, PdfLoad) {
  ASSERT_TRUE(EventsEmpty());

  first_web_state_.SetContentsMimeType("application/pdf");
  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbPdfLoad))
      << events.back();
}

// Tests page load success.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadSuccess) {
  ASSERT_TRUE(EventsEmpty());

  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_FALSE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests page load failure.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadFailure) {
  ASSERT_TRUE(EventsEmpty());

  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests NTP page load.
TEST_F(BreadcrumbManagerTabHelperTest, NtpPageLoad) {
  ASSERT_TRUE(EventsEmpty());

  first_web_state_.SetCurrentURL(GURL(kChromeUINewTabURL));
  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbNtpNavigation))
      << events.back();
  // NTP navigation can't fail, so there is no success/failure metadata.
  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbNtpNavigation))
      << events.back();
}

// Tests navigation error.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationError) {
  ASSERT_TRUE(EventsEmpty());

  web::FakeNavigationContext context;
  NSError* error = web::testing::CreateTestNetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNotConnectedToInternet
             userInfo:nil]);
  context.SetError(error);
  first_web_state_.OnNavigationFinished(&context);
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());

  EXPECT_TRUE(base::Contains(events.back(),
                             breadcrumbs::kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_TRUE(base::Contains(
      events.back(), net::ErrorToShortString(net::ERR_INTERNET_DISCONNECTED)))
      << events.back();
}

// Tests changes in security states.
TEST_F(BreadcrumbManagerTabHelperTest, DidChangeVisibleSecurityState) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web::FakeNavigationManager* navigation_manager_ptr = navigation_manager.get();
  first_web_state_.SetNavigationManager(std::move(navigation_manager));
  ASSERT_TRUE(EventsEmpty());

  // Empty navigation manager.
  first_web_state_.OnVisibleSecurityStateChanged();
  ASSERT_TRUE(EventsEmpty());

  // Default navigation item.
  auto visible_item = web::NavigationItem::Create();
  navigation_manager_ptr->SetVisibleItem(visible_item.get());
  first_web_state_.OnVisibleSecurityStateChanged();
  ASSERT_TRUE(EventsEmpty());

  // Mixed content.
  web::SSLStatus& status = visible_item->GetSSL();
  status.content_status = web::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  first_web_state_.OnVisibleSecurityStateChanged();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbMixedContent))
      << events.back();
  EXPECT_FALSE(base::Contains(events.back(),
                              breadcrumbs::kBreadcrumbAuthenticationBroken))
      << events.back();

  // Broken authentication.
  status.content_status = web::SSLStatus::NORMAL_CONTENT;
  status.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  first_web_state_.OnVisibleSecurityStateChanged();
  ASSERT_EQ(2u, events.size());
  EXPECT_FALSE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbMixedContent))
      << events.back();
  EXPECT_TRUE(base::Contains(events.back(),
                             breadcrumbs::kBreadcrumbAuthenticationBroken))
      << events.back();
}

// Tests that adding an infobar logs the expected breadcrumb.
TEST_F(BreadcrumbManagerTabHelperTest, AddInfobar) {
  ASSERT_TRUE(EventsEmpty());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
  std::unique_ptr<FakeInfobarDelegate> delegate =
      std::make_unique<FakeInfobarDelegate>(identifier);
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(std::move(delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(infobar));

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(base::Contains(
      events.back(),
      base::StringPrintf("%s%d", breadcrumbs::kBreadcrumbInfobarAdded,
                         identifier)))
      << events.back();
}

// Tests that infobar breadcrumbs specify the infobar type.
TEST_F(BreadcrumbManagerTabHelperTest, InfobarTypes) {
  ASSERT_TRUE(EventsEmpty());

  // Add and remove first infobar.
  InfoBarDelegate::InfoBarIdentifier first_identifier =
      InfoBarDelegate::InfoBarIdentifier::SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
  std::unique_ptr<FakeInfobarDelegate> first_delegate =
      std::make_unique<FakeInfobarDelegate>(first_identifier);
  std::unique_ptr<FakeInfobarIOS> first_infobar =
      std::make_unique<FakeInfobarIOS>(std::move(first_delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(first_infobar));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->RemoveAllInfoBars(/*animate=*/false);

  // Add second infobar.
  InfoBarDelegate::InfoBarIdentifier second_identifier =
      InfoBarDelegate::InfoBarIdentifier::SYNC_ERROR_INFOBAR_DELEGATE_IOS;
  std::unique_ptr<FakeInfobarDelegate> second_delegate =
      std::make_unique<FakeInfobarDelegate>(second_identifier);
  std::unique_ptr<FakeInfobarIOS> second_infobar =
      std::make_unique<FakeInfobarIOS>(std::move(second_delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(second_infobar));

  const auto& events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(events.front(), events.back());
  EXPECT_TRUE(base::Contains(
      events.front(),
      base::StringPrintf("%s%d", breadcrumbs::kBreadcrumbInfobarAdded,
                         first_identifier)))
      << events.back();
  EXPECT_TRUE(base::Contains(
      events.back(),
      base::StringPrintf("%s%d", breadcrumbs::kBreadcrumbInfobarAdded,
                         second_identifier)))
      << events.back();
}

// Tests that removing an infobar without animation logs the expected breadcrumb
// event.
TEST_F(BreadcrumbManagerTabHelperTest, RemoveInfobarNotAnimated) {
  ASSERT_TRUE(EventsEmpty());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  std::unique_ptr<FakeInfobarDelegate> delegate =
      std::make_unique<FakeInfobarDelegate>(identifier);
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(std::move(delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(infobar));

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->RemoveAllInfoBars(/*animate=*/false);

  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(base::Contains(
      events.back(),
      base::StringPrintf("%s%d", breadcrumbs::kBreadcrumbInfobarRemoved,
                         identifier)))
      << events.back();
  EXPECT_TRUE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbInfobarNotAnimated))
      << events.back();
}

// Tests that removing an infobar with animation logs the expected breadcrumb
// event.
TEST_F(BreadcrumbManagerTabHelperTest, RemoveInfobarAnimated) {
  ASSERT_TRUE(EventsEmpty());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  std::unique_ptr<FakeInfobarDelegate> delegate =
      std::make_unique<FakeInfobarDelegate>(identifier);
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(std::move(delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(infobar));

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->RemoveAllInfoBars(/*animate=*/true);

  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(base::Contains(
      events.back(),
      base::StringPrintf("%s%d", breadcrumbs::kBreadcrumbInfobarRemoved,
                         identifier)))
      << events.back();
  EXPECT_FALSE(
      base::Contains(events.back(), breadcrumbs::kBreadcrumbInfobarNotAnimated))
      << events.back();
}

// Tests that replacing an infobar logs the expected breadcrumb event.
TEST_F(BreadcrumbManagerTabHelperTest, ReplaceInfobar) {
  ASSERT_TRUE(EventsEmpty());

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::make_unique<FakeInfobarIOS>());

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::make_unique<FakeInfobarIOS>(),
                   /*replace_existing=*/true);

  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  EXPECT_TRUE(base::Contains(
      events.back(),
      base::StringPrintf("%s%d", breadcrumbs::kBreadcrumbInfobarReplaced,
                         identifier)))
      << events.back();
}

// Tests that replacing an infobar many times only logs the replaced infobar
// breadcrumb at major increments.
TEST_F(BreadcrumbManagerTabHelperTest, SequentialInfobarReplacements) {
  ASSERT_TRUE(EventsEmpty());

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::make_unique<FakeInfobarIOS>());

  for (int replacements = 0; replacements < 500; replacements++) {
    InfoBarManagerImpl::FromWebState(&first_web_state_)
        ->AddInfoBar(std::make_unique<FakeInfobarIOS>(),
                     /*replace_existing=*/true);
  }

  const auto& events = GetEvents();
  // Replacing the infobar 500 times should only log breadcrumbs on the 1st,
  // 2nd, 5th, 20th, 100th, 200th replacement.
  ASSERT_EQ(7u, events.size());

  // The events should contain the number of times the info has been replaced.
  // Validate the last one, which occurs at the 200th replacement.
  std::string expected_event =
      base::StringPrintf("%s%d %d", breadcrumbs::kBreadcrumbInfobarReplaced,
                         InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR, 200);
  EXPECT_TRUE(base::Contains(events.back(), expected_event)) << events.back();
}

// Tests Zoom event.
TEST_F(BreadcrumbManagerTabHelperTest, Zoom) {
  ASSERT_TRUE(EventsEmpty());

  [scroll_view_.delegate scrollViewDidEndZooming:scroll_view_
                                        withView:nil
                                         atScale:0];

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbZoom))
      << events.back();
}

// Tests Scroll event.
TEST_F(BreadcrumbManagerTabHelperTest, Scroll) {
  ASSERT_TRUE(EventsEmpty());

  [scroll_view_.delegate scrollViewDidEndDragging:scroll_view_
                                   willDecelerate:YES];

  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(base::Contains(events.back(), breadcrumbs::kBreadcrumbScroll))
      << events.back();
}

// Tests batching sequential Scroll events.
TEST_F(BreadcrumbManagerTabHelperTest, MultipleScrolls) {
  ASSERT_TRUE(EventsEmpty());

  for (int scroll = 0; scroll < 500; scroll++) {
    [scroll_view_.delegate scrollViewDidEndDragging:scroll_view_
                                     willDecelerate:YES];
  }

  // Scrolling 500 times should only log breadcrumbs on the 1st, 2nd, 5th, 20th,
  // 100th and 200th time.
  const auto& events = GetEvents();
  ASSERT_EQ(6u, events.size());

  // The events should contain the number of times the info has been replaced.
  // Validate the last one, which occurs at the 200th scroll completion.
  std::string expected =
      base::StringPrintf("%s %d", breadcrumbs::kBreadcrumbScroll, 200);
  EXPECT_TRUE(base::Contains(events.back(), expected)) << events.back();
}
