// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/tab_url_provider_impl.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/simple_test_clock.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/tab_url_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kURL0[] = "https://www.example.com/0000";
const char kURL1[] = "https://www.example.com/1111";
const char kURL2[] = "https://www.example.com/2222";
const base::TimeDelta kOneSecond = base::Seconds(1);
const base::TimeDelta kOneMinute = base::Seconds(60);
const base::TimeDelta kOneHour = base::Hours(1);

// Test fixture for TabUrlProviderImpl.
class TabUrlProviderImplTest : public PlatformTest {
 public:
  TabUrlProviderImplTest() = default;
  ~TabUrlProviderImplTest() override = default;

  void SetUp() override {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    other_browser_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(browser_.get());
    browser_list_->AddBrowser(other_browser_.get());
    browser_list_->AddBrowser(incognito_browser_.get());

    tab_url_provider_ =
        std::make_unique<TabUrlProviderImpl>(browser_list_, &clock_);
  }

  // Add a fake web state with certain URL and timestamp to be the last
  // committed navigation.
  void AddURL(Browser* browser, const GURL& url, const base::Time& timestamp) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetCurrentURL(url);
    fake_web_state->SetLastActiveTime(timestamp);
    browser->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) {
    return tab_url_provider_->GetUrlsOfActiveTabs(duration_since_last_shown);
  }

  Browser* browser() { return browser_.get(); }
  Browser* other_browser() { return other_browser_.get(); }
  Browser* incognito_browser() { return incognito_browser_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  base::SimpleTestClock clock_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> other_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;
};

// No incognito tab URLs will be returned.
TEST_F(TabUrlProviderImplTest, NoIncognitoTabURLs) {
  AddURL(incognito_browser(), GURL(kURL0), clock()->Now() - kOneSecond);
  EXPECT_TRUE(GetUrlsOfActiveTabs(kOneMinute).empty());
}

// Expired URL will be pruned.
TEST_F(TabUrlProviderImplTest, NoExpiredURL) {
  AddURL(browser(), GURL(kURL0), clock()->Now() - kOneHour);
  EXPECT_TRUE(GetUrlsOfActiveTabs(kOneMinute).empty());
}

// Non expired active URL is returned.
TEST_F(TabUrlProviderImplTest, ActiveURLReturned) {
  AddURL(browser(), GURL(kURL0), clock()->Now() - kOneSecond);
  auto urls = GetUrlsOfActiveTabs(kOneMinute);
  EXPECT_EQ(1U, urls.size());
  EXPECT_EQ(GURL(kURL0), urls.front());
}

// URLs are sorted by their timestamp. Also tabs in all browsers will be
// checked.
TEST_F(TabUrlProviderImplTest, URLsFromAllBrowsersAreSorted) {
  AddURL(browser(), GURL(kURL0), clock()->Now() - kOneSecond);
  AddURL(browser(), GURL(kURL1), clock()->Now() - 2 * kOneMinute);
  AddURL(other_browser(), GURL(kURL2), clock()->Now() - kOneMinute);

  std::vector<GURL> expected = {GURL(kURL0), GURL(kURL2), GURL(kURL1)};
  EXPECT_EQ(expected, GetUrlsOfActiveTabs(kOneHour));
}

}  // namespace
