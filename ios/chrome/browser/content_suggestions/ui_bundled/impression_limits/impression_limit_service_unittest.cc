// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service_factory.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
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

}  // namespace

class ImpressionLimitServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ImpressionLimitServiceFactory::GetInstance(),
        base::BindRepeating(
            [](PrefService* pref_service, web::BrowserState* browser_state)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<ImpressionLimitService>(
                  pref_service, ios::HistoryServiceFactory::GetForProfile(
                                    ProfileIOS::FromBrowserState(browser_state),
                                    ServiceAccessType::EXPLICIT_ACCESS));
            },
            pref_service()));
    profile_ = std::move(builder).Build();
    pref_service_.registry()->RegisterDictionaryPref(kImpressionsPref);
    impression_limit_service_ =
        ImpressionLimitServiceFactory::GetForProfile(profile_.get());
  }

  PrefService* pref_service() { return &pref_service_; }

  ProfileIOS* profile() { return profile_.get(); }

  ImpressionLimitService* service() { return impression_limit_service_; }

  void LogImpressionForURLAtTime(const GURL& url,
                                 const std::string_view& pref_name,
                                 base::Time impression_time) {
    service()->LogImpressionForURLAtTime(url, pref_name, impression_time);
  }

  void RemoveEntriesBeforeTime(const std::string_view& pref_name,
                               base::Time before_cutoff) {
    service()->RemoveEntriesBeforeTime(pref_name, before_cutoff);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<ImpressionLimitService> impression_limit_service_;
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
