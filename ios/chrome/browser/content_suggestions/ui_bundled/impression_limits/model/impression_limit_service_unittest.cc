// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/model/impression_limit_service.h"

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/mock_subscriptions_manager.h"
#include "components/commerce/core/test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#include "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/model/impression_limit_service_factory.h"
#include "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/model/shop_card_prefs.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

base::Time TimeFromString(const char* str) {
  base::Time time;
  EXPECT_TRUE(base::Time::FromString(str, &time));
  return time;
}

constexpr char kImpressionsPref[] = "tab_resumption.price_drop.url_impressions";
constexpr char kGurl1[] = "https://example.com/one";
constexpr char kGurl2[] = "https://example.com/two";
constexpr char kGurl3[] = "https://example.com/three";
constexpr char kGurl1WithQuery[] = "https://example.com/one?key=value";
constexpr char kGurl1WithRef[] = "https://example.com/one#ref";
constexpr char kGurl1WithQueryAndRef[] =
    "https://example.com/one?key=value#ref";
const base::Time kNow = TimeFromString("31 Mar 2025 10:00");
const base::Time kYesterday = TimeFromString("30 Mar 2025 10:30");
const base::Time kLastMonth = TimeFromString("25 Feb 2025 9:00");
const uint64_t kValidId = 67890L;

const char* kPrefsToRegister[] = {
    shop_card_prefs::kShopCardPriceDropUrlImpressions,
    tab_resumption_prefs::kTabResumptionRegularUrlImpressions,
    tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions,
    tab_resumption_prefs::kTabResumptionWithPriceTrackableUrlImpressions,
};

history::URLRows CreateURLRows(const std::vector<GURL>& urls) {
  history::URLRows url_rows;
  for (const auto& url : urls) {
    url_rows.emplace_back(history::URLRow(url));
  }
  return url_rows;
}

}  // namespace

class ImpressionLimitServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features*/ {commerce::kShopCardImpressionLimits},
        /* disabled_features*/ {});
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return commerce::MockShoppingService::Build();
            }));
    builder.AddTestingFactory(
        ImpressionLimitServiceFactory::GetInstance(),
        base::BindRepeating(
            [](PrefService* pref_service,
               bookmarks::BookmarkModel* bookmark_model,
               ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<ImpressionLimitService>(
                  pref_service,
                  ios::HistoryServiceFactory::GetForProfile(
                      profile, ServiceAccessType::EXPLICIT_ACCESS),
                  bookmark_model,
                  commerce::ShoppingServiceFactory::GetForProfile(profile));
            },
            pref_service(), bookmark_model_.get()));

    profile_ = std::move(builder).Build();

    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForProfile(profile_.get()));
    for (auto* const pref : kPrefsToRegister) {
      pref_service_.registry()->RegisterDictionaryPref(pref);
    }
    impression_limit_service_ =
        ImpressionLimitServiceFactory::GetForProfile(profile_.get());
  }

  PrefService* pref_service() { return &pref_service_; }

  ProfileIOS* profile() { return profile_.get(); }

  ImpressionLimitService* service() { return impression_limit_service_.get(); }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

  commerce::MockShoppingService* shopping_service() {
    return shopping_service_;
  }

  void LogImpressionForURLAtTime(const GURL& url,
                                 const std::string_view& pref_name,
                                 base::Time impression_time) {
    service()->LogImpressionForURLAtTime(url, pref_name, impression_time);
  }

  void RemoveEntriesBeforeTime(const std::string_view& pref_name,
                               base::Time before_cutoff) {
    service()->RemoveEntriesBeforeTime(pref_name, before_cutoff);
  }

  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) {
    service()->OnHistoryDeletions(history_service, deletion_info);
  }

  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) {
    service()->BookmarkNodeRemoved(parent, old_index, node,
                                   no_longer_bookmarked, location);
  }

  void OnUnsubscribe(const commerce::CommerceSubscription& subscription,
                     bool succeeded) {
    service()->OnUnsubscribe(subscription, succeeded);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<ImpressionLimitService> impression_limit_service_;
  raw_ptr<commerce::MockShoppingService> shopping_service_;
};

// Check Impressions are logged correctly for same URL.
TEST_F(ImpressionLimitServiceTest, TestLoggingImpressions) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kLastMonth);
  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kYesterday);
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kNow);
  count = service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(3, count.value());
}

// Check removing impressions for a URL after logging impressions
// for several URLs.
TEST_F(ImpressionLimitServiceTest, TestRemove) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kLastMonth);
  LogImpressionForURLAtTime(GURL(kGurl2), kImpressionsPref, kYesterday);
  LogImpressionForURLAtTime(GURL(kGurl3), kImpressionsPref, kNow);
  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  count = service()->GetImpressionCount(GURL(kGurl3), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());

  RemoveEntriesBeforeTime(kImpressionsPref, kNow - base::Days(30));
  count = service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  count = service()->GetImpressionCount(GURL(kGurl3), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
}

// Check URLs are stripped in storage (i.e. ref tag and query string
// removed) i.e. impressions for https://example.com/ are stored the
// same as https://example.com/?foo=bar#ref.
TEST_F(ImpressionLimitServiceTest, TestUrlStripping) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kLastMonth);
  LogImpressionForURLAtTime(GURL(kGurl1WithQuery), kImpressionsPref,
                            kLastMonth);
  LogImpressionForURLAtTime(GURL(kGurl1WithRef), kImpressionsPref, kYesterday);
  LogImpressionForURLAtTime(GURL(kGurl1WithQueryAndRef), kImpressionsPref,
                            kNow);

  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(4, count.value());
}

// Test deleting a specific URL from history deletes the corresponding
// URL in ImpressionLimitService.
TEST_F(ImpressionLimitServiceTest, TestHistoryDelete) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kNow);
  LogImpressionForURLAtTime(GURL(kGurl2), kImpressionsPref, kNow);
  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  OnHistoryDeletions(
      nil, history::DeletionInfo::ForUrls(CreateURLRows({GURL(kGurl1)}),
                                          /*favicon_urls=*/{}));
  count = service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
}

// Test deleting all of history deletes all stored URLs in
// ImpressionLimitService.
TEST_F(ImpressionLimitServiceTest, TestHistoryAllDelete) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kYesterday);
  LogImpressionForURLAtTime(GURL(kGurl1WithQueryAndRef), kImpressionsPref,
                            kLastMonth);
  LogImpressionForURLAtTime(GURL(kGurl2), kImpressionsPref, kNow);
  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(2, count.value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  OnHistoryDeletions(nil, history::DeletionInfo::ForAllHistory());
  count = service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_FALSE(count.has_value());
}

// Test deleting a bookmark deletes the URL corresponding to the bookmark
// in ImpressionLimitService. Both a URL and the same URL with query string
// ref tag are tested.
TEST_F(ImpressionLimitServiceTest, TestBookmarkDelete) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kNow);
  LogImpressionForURLAtTime(GURL(kGurl1WithQueryAndRef), kImpressionsPref,
                            kLastMonth);
  LogImpressionForURLAtTime(GURL(kGurl2), kImpressionsPref, kYesterday);
  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(2, count.value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
  BookmarkNodeRemoved(nil, -1, nil, {GURL(kGurl1)}, FROM_HERE);
  count = service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl1WithQueryAndRef),
                                        kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
}

// Test unsubscribing from a product URL deletes the URL corresponding to
// the product URL in ImpressionLimitService. Both a URL and the same URL
// with query string ref tag are tested.
TEST_F(ImpressionLimitServiceTest, TestUnubscribe) {
  LogImpressionForURLAtTime(GURL(kGurl1), kImpressionsPref, kNow);
  LogImpressionForURLAtTime(GURL(kGurl1WithQueryAndRef), kImpressionsPref,
                            kLastMonth);
  LogImpressionForURLAtTime(GURL(kGurl2), kImpressionsPref, kYesterday);

  std::optional<int> count =
      service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(2, count.value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());

  std::vector<commerce::CommerceSubscription> subs;
  subs.push_back(commerce::BuildUserSubscriptionForClusterId(kValidId));
  commerce::CommerceSubscription subscription = subs[0];
  shopping_service()->SetGetAllSubscriptionsCallbackValue(std::move(subs));
  shopping_service()->SetUnsubscribeCallbackValue(true);

  // Ensure there's a bookmark for the above subscription.
  commerce::AddProductBookmark(bookmark_model(), u"product", GURL(kGurl1),
                               kValidId, true);

  OnUnsubscribe(subscription, true);
  count = service()->GetImpressionCount(GURL(kGurl1), kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl1WithQueryAndRef),
                                        kImpressionsPref);
  EXPECT_FALSE(count.has_value());
  count = service()->GetImpressionCount(GURL(kGurl2), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(1, count.value());
}

// Long URLs are clipped at 1024 characters. Check a URL exceeding
// the maximum length is stored under the same key as the first
// 1024 characters of the same URL.
TEST_F(ImpressionLimitServiceTest, TestLongUrl) {
  std::string long_url = "https://www.example.com/";
  for (int i = 0; i < 1000; i++) {
    long_url += "abcdefghi/";
  }
  LogImpressionForURLAtTime(GURL(long_url), kImpressionsPref, kYesterday);
  // First 1024 characters of URL should be logged under same key.
  std::string shortened_long_url = long_url.substr(0, 1024);
  EXPECT_TRUE(long_url.size() > shortened_long_url.size());
  LogImpressionForURLAtTime(GURL(shortened_long_url), kImpressionsPref, kNow);

  std::optional<int> count =
      service()->GetImpressionCount(GURL(long_url), kImpressionsPref);
  EXPECT_TRUE(count.has_value());
  EXPECT_EQ(2, count.value());
}

// Test if we exceed 10 entries, we remove the oldest (in any
// preference).
TEST_F(ImpressionLimitServiceTest, TestMaximumEntries) {
  // Add 10 entries
  for (int i = 1; i <= 10; i++) {
    LogImpressionForURLAtTime(
        GURL(base::StringPrintf("https://www.example.com/%d/", i)),
        kImpressionsPref, kNow + base::Hours(i));
  }
  // Check the 10 entries
  for (int i = 1; i <= 10; i++) {
    std::optional<int> count = service()->GetImpressionCount(
        GURL(base::StringPrintf("https://www.example.com/%d/", i)),
        kImpressionsPref);
    EXPECT_TRUE(count.has_value());
    EXPECT_EQ(1, count.value());
  }
  // Add 11th entry
  LogImpressionForURLAtTime(
      GURL(base::StringPrintf("https://www.example.com/%d/", 11)),
      kImpressionsPref, kNow + base::Hours(11));
  for (int i = 1; i <= 11; i++) {
    std::optional<int> count = service()->GetImpressionCount(
        GURL(base::StringPrintf("https://www.example.com/%d/", i)),
        kImpressionsPref);
    // First entry (oldest) should have been removed)
    if (i == 1) {
      EXPECT_FALSE(count.has_value());
    } else {
      // All other entries, including the one just added should be there.
      EXPECT_TRUE(count.has_value());
      EXPECT_EQ(1, count.value());
    }
  }
}
