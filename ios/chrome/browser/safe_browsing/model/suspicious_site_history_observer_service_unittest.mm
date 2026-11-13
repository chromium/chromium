// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/suspicious_site_history_observer_service.h"

#import <memory>
#import <string>
#import <string_view>

#import "base/time/time.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/history/core/browser/history_types.h"
#import "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/suspicious_site_history_observer_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

constexpr std::string_view kAllowedHost1 = "suspicious1.chromium.test";
constexpr std::string_view kAllowedHost2 = "suspicious2.chromium.test";
constexpr std::string_view kAllowedUrl1 =
    "https://suspicious1.chromium.test/page";

}  // namespace

class SuspiciousSiteHistoryObserverServiceTest : public PlatformTest {
 protected:
  SuspiciousSiteHistoryObserverServiceTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SuspiciousSiteHistoryObserverServiceFactory::GetInstance(),
        SuspiciousSiteHistoryObserverServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    service_ = SuspiciousSiteHistoryObserverServiceFactory::GetForProfile(
        profile_.get());
  }

  scoped_refptr<HostContentSettingsMap> host_content_settings_map() {
    return ios::HostContentSettingsMapFactory::GetForProfile(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<SuspiciousSiteHistoryObserverService> service_ = nullptr;
};

// Tests that `SuspiciousSiteHistoryObserverServiceFactory` creates a service
// instance for a regular profile and returns `nullptr` for an Off-The-Record
// profile.
TEST_F(SuspiciousSiteHistoryObserverServiceTest,
       FactoryOffTheRecordReturnsNull) {
  EXPECT_NE(service_, nullptr);
  EXPECT_EQ(SuspiciousSiteHistoryObserverServiceFactory::GetForProfile(
                profile_->GetOffTheRecordProfile()),
            nullptr);
}

// Tests that `OnHistoryDeletions` clears all allowlisted hosts when all
// browsing history is deleted.
TEST_F(SuspiciousSiteHistoryObserverServiceTest,
       OnHistoryDeletionsClearsAllHostsForAllHistory) {
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist(
      host_content_settings_map());
  allowlist.AllowSiteForHost(std::string(kAllowedHost1));
  allowlist.AllowSiteForHost(std::string(kAllowedHost2));
  ASSERT_TRUE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost1)));
  ASSERT_TRUE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost2)));

  ASSERT_NE(service_, nullptr);
  service_->OnHistoryDeletions(/*history_service=*/nullptr,
                               history::DeletionInfo::ForAllHistory());

  EXPECT_FALSE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost1)));
  EXPECT_FALSE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost2)));
}

// Tests that `OnHistoryDeletions` revokes the allowlist entry for a deleted URL
// when zero visits remain for its host, while preserving other hosts.
TEST_F(SuspiciousSiteHistoryObserverServiceTest,
       OnHistoryDeletionsRevokesHostWhenZeroVisitsRemain) {
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist(
      host_content_settings_map());
  allowlist.AllowSiteForHost(std::string(kAllowedHost1));
  allowlist.AllowSiteForHost(std::string(kAllowedHost2));

  const GURL deleted_url(kAllowedUrl1);
  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {history::URLRow(deleted_url)}, /*favicon_urls=*/{});
  deletion_info.set_deleted_urls_origin_map(
      {{deleted_url.DeprecatedGetOriginAsURL(), {0, base::Time::Now()}}});

  ASSERT_NE(service_, nullptr);
  service_->OnHistoryDeletions(/*history_service=*/nullptr, deletion_info);

  EXPECT_FALSE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost1)));
  EXPECT_TRUE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost2)));
}

// Tests that `OnHistoryDeletions` retains the allowlist entry for a host when
// at least one visit to that host remains in history.
TEST_F(SuspiciousSiteHistoryObserverServiceTest,
       OnHistoryDeletionsRetainsHostWhenVisitsRemain) {
  safe_browsing::SuspiciousSiteWarningAllowlist allowlist(
      host_content_settings_map());
  allowlist.AllowSiteForHost(std::string(kAllowedHost1));

  const GURL deleted_url(kAllowedUrl1);
  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {history::URLRow(deleted_url)}, /*favicon_urls=*/{});
  deletion_info.set_deleted_urls_origin_map(
      {{deleted_url.DeprecatedGetOriginAsURL(), {1, base::Time::Now()}}});

  ASSERT_NE(service_, nullptr);
  service_->OnHistoryDeletions(/*history_service=*/nullptr, deletion_info);

  EXPECT_TRUE(allowlist.IsSiteAllowedForHost(std::string(kAllowedHost1)));
}
