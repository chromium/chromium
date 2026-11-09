// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_blocking_page.h"

#import <memory>
#import <string>
#import <string_view>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/values.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "components/security_interstitials/core/controller_client.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"
#import "ios/chrome/browser/enterprise/proxy/model/fake_enterprise_proxy_tab_helper_delegate.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/http/http_status_code.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

using ::testing::HasSubstr;
using ErrorCategory = enterprise_net::EnterpriseProxyErrorData::ErrorCategory;

constexpr ErrorCategory kAllCategories[] = {ErrorCategory::kAuthentication,
                                            ErrorCategory::kAuthorization,
                                            ErrorCategory::kOther};

// Histogram of the page events recorded for all the error categories.
constexpr std::string_view kEventHistogram =
    "Enterprise.SecureGateway.ErrorPage.Event";

constexpr std::string_view kDestinationUrl = "https://corp.example.com/";
constexpr std::string_view kPreviousUrl = "https://previous.example.com/";
constexpr std::string_view kProxyUrl = "http://proxy.example.com:8080/";
// The page only depends on the error category, not on the error code.
constexpr int kErrorCode = net::HTTP_FORBIDDEN;

// Keys of the template parameters returned by
// `EnterpriseProxyErrorService::GetErrorPageParams()`.
constexpr std::string_view kTitleKey = "title";
constexpr std::string_view kHeadingKey = "heading";
constexpr std::string_view kPrimaryParagraphKey = "primary_paragraph";
constexpr std::string_view kButtonTextKey = "button_text";
constexpr std::string_view kErrorCategoryKey = "error_category";

constexpr std::string_view kTitle = "Test title";
constexpr std::string_view kHeading = "Test heading";
constexpr std::string_view kPrimaryParagraph = "Test primary paragraph";
constexpr std::string_view kButtonText = "Test button";

// Declaration of the template parameters exposed to the page script.
constexpr std::string_view kLoadTimeDataRawDeclaration = "var loadTimeDataRaw";

// Index of the interstitial in the navigation history when there is a previous
// page to go back to.
constexpr int kInterstitialItemIndex = 1;
// Index of the previous page in the navigation history.
constexpr int kPreviousItemIndex = 0;

// Returns the value of the template `error_category` parameter for `category`.
std::string GetErrorCategoryParam(ErrorCategory category) {
  return base::NumberToString(static_cast<int>(category));
}

// Returns the template parameters of the page for `category`.
base::DictValue CreateErrorPageParams(ErrorCategory category) {
  return base::DictValue()
      .Set(kTitleKey, kTitle)
      .Set(kHeadingKey, kHeading)
      .Set(kPrimaryParagraphKey, kPrimaryParagraph)
      .Set(kButtonTextKey, kButtonText)
      .Set(kErrorCategoryKey, GetErrorCategoryParam(category));
}

// Returns the name of the event histogram recorded for the error `category`
// only.
std::string GetCategoryHistogram(ErrorCategory category) {
  std::string_view suffix;
  switch (category) {
    case ErrorCategory::kAuthentication:
      suffix = "Authentication";
      break;
    case ErrorCategory::kAuthorization:
      suffix = "Authorization";
      break;
    case ErrorCategory::kOther:
      suffix = "Other";
      break;
  }
  return base::StrCat({kEventHistogram, ".", suffix});
}

// Expects `count` samples of `event` recorded for an error of `category`, both
// for all the error categories and for `category`.
void ExpectEventCount(const base::HistogramTester& histogram_tester,
                      ErrorCategory category,
                      SecureGatewayErrorPageEvent event,
                      int count) {
  histogram_tester.ExpectBucketCount(kEventHistogram, event, count);
  histogram_tester.ExpectBucketCount(GetCategoryHistogram(category), event,
                                     count);
}

class EnterpriseProxyBlockingPageTest : public PlatformTest {
 protected:
  EnterpriseProxyBlockingPageTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    EnterpriseProxyTabHelper::CreateForWebState(&web_state_, &error_service_);
    EnterpriseProxyTabHelper::FromWebState(&web_state_)
        ->SetDelegate(&delegate_);
  }

  // Creates the page displayed for an error of `category`.
  std::unique_ptr<EnterpriseProxyBlockingPage> CreatePage(
      ErrorCategory category) {
    return EnterpriseProxyBlockingPage::Create(
        &web_state_,
        enterprise_net::EnterpriseProxyErrorData(
            GURL(kDestinationUrl), GURL(kProxyUrl), kErrorCode, category),
        CreateErrorPageParams(category));
  }

  // Adds a previous page and the interstitial to the navigation history.
  void AddHistoryWithPreviousPage() {
    navigation_manager_->AddItem(GURL(kPreviousUrl), ui::PAGE_TRANSITION_TYPED);
    navigation_manager_->AddItem(GURL(kDestinationUrl),
                                 ui::PAGE_TRANSITION_LINK);
  }

  // Displays `page` as the error page of a committed navigation, so that the
  // `IOSBlockingPageTabHelper` of `web_state_` owns it, as in production.
  void DisplayPage(std::unique_ptr<EnterpriseProxyBlockingPage> page) {
    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        &web_state_);
    web::FakeNavigationContext context;
    context.SetWebState(&web_state_);
    blocking_page_tab_helper()->AssociateBlockingPage(context.GetNavigationId(),
                                                      std::move(page));
    context.SetHasCommitted(true);
    web_state_.OnNavigationFinished(&context);
  }

  // Starts and commits a navigation with `transition` in `web_state_`.
  void Navigate(ui::PageTransition transition) {
    web::FakeNavigationContext context;
    context.SetWebState(&web_state_);
    context.SetPageTransition(transition);
    web_state_.OnNavigationStarted(&context);
    context.SetHasCommitted(true);
    web_state_.OnNavigationFinished(&context);
  }

  // Returns the tab helper that owns the pages passed to `DisplayPage()`.
  security_interstitials::IOSBlockingPageTabHelper* blocking_page_tab_helper() {
    return security_interstitials::IOSBlockingPageTabHelper::FromWebState(
        &web_state_);
  }

  web::WebTaskEnvironment task_environment_;
  testing::NiceMock<enterprise_net::MockEnterpriseProxyService> proxy_service_;
  // Outlive `web_state_`, whose tab helper points to them.
  enterprise_net::EnterpriseProxyErrorService error_service_{&proxy_service_};
  FakeEnterpriseProxyTabHelperDelegate delegate_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
};

}  // namespace

// Test that the page renders the template parameters of each error category,
// and exposes them to the page script through `loadTimeDataRaw`, from which it
// reads the error category to pick the button action.
TEST_F(EnterpriseProxyBlockingPageTest, RendersErrorPageParams) {
  for (ErrorCategory category : kAllCategories) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));

    std::string html = CreatePage(category)->GetHtmlContents();

    EXPECT_THAT(html, HasSubstr(base::StrCat({"<h1>", kHeading, "</h1>"})));
    EXPECT_THAT(html,
                HasSubstr(base::StrCat({"<p>", kPrimaryParagraph, "</p>"})));
    EXPECT_THAT(html, HasSubstr(std::string(kButtonText)));
    EXPECT_THAT(html, HasSubstr(std::string(kLoadTimeDataRawDeclaration)));
    EXPECT_THAT(
        html, HasSubstr(base::StrCat({"\"", kErrorCategoryKey, "\":\"",
                                      GetErrorCategoryParam(category), "\""})));
  }
}

// Test that the page title is rendered from the `title` parameter.
TEST_F(EnterpriseProxyBlockingPageTest, RendersTitle) {
  std::string html =
      CreatePage(ErrorCategory::kAuthentication)->GetHtmlContents();

  EXPECT_THAT(html, HasSubstr(base::StrCat({"<title>", kTitle, "</title>"})));
}

// Test that "Go back" navigates back to the previous page.
TEST_F(EnterpriseProxyBlockingPageTest, GoBackNavigatesBack) {
  AddHistoryWithPreviousPage();
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kAuthorization);

  page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);

  EXPECT_EQ(kPreviousItemIndex,
            navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_FALSE(web_state_.IsClosed());
}

// Test that "Go back" closes the tab when there is no page to go back to.
TEST_F(EnterpriseProxyBlockingPageTest, GoBackClosesTabWithoutPreviousPage) {
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kOther);

  page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);

  // The tab is closed asynchronously.
  EXPECT_TRUE(base::test::RunUntil([&]() { return web_state_.IsClosed(); }));
}

// Test that "Continue" neither navigates back nor closes the tab, whatever the
// error category.
TEST_F(EnterpriseProxyBlockingPageTest, ContinueDoesNotNavigate) {
  AddHistoryWithPreviousPage();
  for (ErrorCategory category : kAllCategories) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));
    std::unique_ptr<EnterpriseProxyBlockingPage> page = CreatePage(category);

    page->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);

    EXPECT_EQ(kInterstitialItemIndex,
              navigation_manager_->GetLastCommittedItemIndex());
    EXPECT_FALSE(web_state_.IsClosed());
  }
}

// Tests that "Continue" on the authentication page forwards a re-sign-in
// request to the tab helper delegate.
TEST_F(EnterpriseProxyBlockingPageTest, ContinueRequestsSignIn) {
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kAuthentication);

  page->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);

  EXPECT_EQ(1, delegate_.sign_in_count());
}

// Tests that only the authentication page forwards re-sign-in requests.
TEST_F(EnterpriseProxyBlockingPageTest, IgnoresSignInOutsideAuthentication) {
  for (ErrorCategory category :
       {ErrorCategory::kAuthorization, ErrorCategory::kOther}) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));

    CreatePage(category)->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);

    EXPECT_EQ(0, delegate_.sign_in_count());
  }
}

// Test that creating the page, right before it's displayed, records it as
// shown, both for all the error categories and for its category only.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsShown) {
  for (ErrorCategory category : kAllCategories) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));
    base::HistogramTester histogram_tester;

    std::unique_ptr<EnterpriseProxyBlockingPage> page = CreatePage(category);

    histogram_tester.ExpectUniqueSample(kEventHistogram,
                                        SecureGatewayErrorPageEvent::kShown, 1);
    for (ErrorCategory histogram_category : kAllCategories) {
      if (histogram_category == category) {
        histogram_tester.ExpectUniqueSample(
            GetCategoryHistogram(histogram_category),
            SecureGatewayErrorPageEvent::kShown, 1);
      } else {
        histogram_tester.ExpectTotalCount(
            GetCategoryHistogram(histogram_category), 0);
      }
    }
  }
}

// Test that "Go back" is recorded, and that leaving the page with the back
// navigation it starts isn't also recorded as a dismissal.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsGoBack) {
  AddHistoryWithPreviousPage();
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kAuthorization);
  base::HistogramTester histogram_tester;

  page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);
  // Destroyed by `IOSBlockingPageTabHelper` when the back navigation commits.
  page.reset();

  ExpectEventCount(histogram_tester, ErrorCategory::kAuthorization,
                   SecureGatewayErrorPageEvent::kGoBack, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that "Go back" is recorded when it closes the tab, and that closing the
// tab isn't also recorded as a dismissal.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsGoBackClosingTab) {
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kOther);
  base::HistogramTester histogram_tester;

  page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);
  ASSERT_TRUE(base::test::RunUntil([&]() { return web_state_.IsClosed(); }));
  // Destroyed with the closed tab.
  page.reset();

  ExpectEventCount(histogram_tester, ErrorCategory::kOther,
                   SecureGatewayErrorPageEvent::kGoBack, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that "Continue" on the authentication page is recorded once, however
// many times it is tapped, and that leaving the page afterwards isn't recorded
// as a dismissal. Each tap still requests a sign-in.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsContinueOnce) {
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kAuthentication);
  base::HistogramTester histogram_tester;

  page->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);
  page->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);
  page.reset();

  ExpectEventCount(histogram_tester, ErrorCategory::kAuthentication,
                   SecureGatewayErrorPageEvent::kContinue, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
  EXPECT_EQ(2, delegate_.sign_in_count());
}

// Test that "Continue" is recorded even when the sign-in request goes nowhere,
// as without a tab helper delegate, since it measures the decision of the user.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsContinueWithoutDelegate) {
  EnterpriseProxyTabHelper::FromWebState(&web_state_)->SetDelegate(nullptr);
  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      CreatePage(ErrorCategory::kAuthentication);
  base::HistogramTester histogram_tester;

  page->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);
  page.reset();

  ExpectEventCount(histogram_tester, ErrorCategory::kAuthentication,
                   SecureGatewayErrorPageEvent::kContinue, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that "Continue" is ignored outside the authentication page, so that
// leaving the page is still recorded as a dismissal.
TEST_F(EnterpriseProxyBlockingPageTest, IgnoresContinueOutsideAuthentication) {
  for (ErrorCategory category :
       {ErrorCategory::kAuthorization, ErrorCategory::kOther}) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));
    std::unique_ptr<EnterpriseProxyBlockingPage> page = CreatePage(category);
    // Called by `IOSBlockingPageTabHelper` when the page is displayed.
    page->ShowInfobar();
    base::HistogramTester histogram_tester;

    page->HandleCommand(security_interstitials::CMD_OPEN_LOGIN);
    page.reset();

    ExpectEventCount(histogram_tester, category,
                     SecureGatewayErrorPageEvent::kContinue, 0);
    ExpectEventCount(histogram_tester, category,
                     SecureGatewayErrorPageEvent::kDismissed, 1);
  }
}

// Test that destroying a displayed page without an outcome, which happens when
// it is left without using its button, records a dismissal.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsDismissalOnDestruction) {
  for (ErrorCategory category : kAllCategories) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));
    std::unique_ptr<EnterpriseProxyBlockingPage> page = CreatePage(category);
    // Called by `IOSBlockingPageTabHelper` when the page is displayed.
    page->ShowInfobar();
    base::HistogramTester histogram_tester;

    page.reset();

    ExpectEventCount(histogram_tester, category,
                     SecureGatewayErrorPageEvent::kDismissed, 1);
    histogram_tester.ExpectTotalCount(kEventHistogram, 1);
  }
}

// Test that destroying a page that was never displayed records it as not
// displayed rather than dismissed.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsNotDisplayedOnDestruction) {
  for (ErrorCategory category : kAllCategories) {
    SCOPED_TRACE(testing::Message()
                 << "category: " << static_cast<int>(category));
    std::unique_ptr<EnterpriseProxyBlockingPage> page = CreatePage(category);
    base::HistogramTester histogram_tester;

    page.reset();

    ExpectEventCount(histogram_tester, category,
                     SecureGatewayErrorPageEvent::kNotDisplayed, 1);
    histogram_tester.ExpectTotalCount(kEventHistogram, 1);
  }
}

// Test that a page whose navigation doesn't commit, so that it's never
// displayed, is recorded as not displayed when `IOSBlockingPageTabHelper` drops
// it.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsNotDisplayedWhenNotCommitted) {
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      &web_state_);
  web::FakeNavigationContext context;
  context.SetWebState(&web_state_);
  blocking_page_tab_helper()->AssociateBlockingPage(
      context.GetNavigationId(), CreatePage(ErrorCategory::kOther));
  base::HistogramTester histogram_tester;

  web_state_.OnNavigationFinished(&context);

  ExpectEventCount(histogram_tester, ErrorCategory::kOther,
                   SecureGatewayErrorPageEvent::kNotDisplayed, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that a back navigation that doesn't come from "Go back", e.g. from the
// back button of the browser, records a dismissal once it commits.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsBackNavigationAsDismissal) {
  DisplayPage(CreatePage(ErrorCategory::kAuthorization));
  base::HistogramTester histogram_tester;

  Navigate(ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                     ui::PAGE_TRANSITION_FORWARD_BACK));

  ExpectEventCount(histogram_tester, ErrorCategory::kAuthorization,
                   SecureGatewayErrorPageEvent::kDismissed, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that a back navigation that doesn't commit leaves the outcome open, so
// that the button tapped afterwards is still recorded.
TEST_F(EnterpriseProxyBlockingPageTest, IgnoresBackNavigationNotCommitted) {
  AddHistoryWithPreviousPage();
  DisplayPage(CreatePage(ErrorCategory::kOther));
  base::HistogramTester histogram_tester;

  web::FakeNavigationContext context;
  context.SetWebState(&web_state_);
  context.SetPageTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FORWARD_BACK));
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);

  histogram_tester.ExpectTotalCount(kEventHistogram, 0);

  blocking_page_tab_helper()->OnBlockingPageCommandReceived(
      security_interstitials::CMD_DONT_PROCEED);

  ExpectEventCount(histogram_tester, ErrorCategory::kOther,
                   SecureGatewayErrorPageEvent::kGoBack, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that "Go back" is only recorded once when its back navigation goes
// through `IOSBlockingPageTabHelper`.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsGoBackThroughTabHelper) {
  AddHistoryWithPreviousPage();
  DisplayPage(CreatePage(ErrorCategory::kOther));
  base::HistogramTester histogram_tester;

  blocking_page_tab_helper()->OnBlockingPageCommandReceived(
      security_interstitials::CMD_DONT_PROCEED);
  // The back navigation started by "Go back".
  Navigate(ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                     ui::PAGE_TRANSITION_FORWARD_BACK));

  ExpectEventCount(histogram_tester, ErrorCategory::kOther,
                   SecureGatewayErrorPageEvent::kGoBack, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}

// Test that leaving the page with a navigation that isn't a back navigation,
// e.g. from the omnibox, records a dismissal once it commits.
TEST_F(EnterpriseProxyBlockingPageTest, RecordsOtherNavigationAsDismissal) {
  DisplayPage(CreatePage(ErrorCategory::kAuthentication));
  base::HistogramTester histogram_tester;

  Navigate(ui::PAGE_TRANSITION_TYPED);

  ExpectEventCount(histogram_tester, ErrorCategory::kAuthentication,
                   SecureGatewayErrorPageEvent::kDismissed, 1);
  histogram_tester.ExpectTotalCount(kEventHistogram, 1);
}
