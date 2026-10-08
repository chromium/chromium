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
#import "base/test/run_until.h"
#import "base/values.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/security_interstitials/core/controller_client.h"
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

class EnterpriseProxyBlockingPageTest : public PlatformTest {
 protected:
  EnterpriseProxyBlockingPageTest() {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
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

  web::WebTaskEnvironment task_environment_;
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
