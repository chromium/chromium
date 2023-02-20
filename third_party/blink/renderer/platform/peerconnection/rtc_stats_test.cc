// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/stats/test/rtc_test_stats.h"

namespace blink {

namespace {

// Stats object with both a standard and non-standard member, used for the test
// below.
class TestStats : public webrtc::RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  TestStats(const std::string& id, webrtc::Timestamp timestamp);
  ~TestStats() override = default;

  webrtc::RTCStatsMember<int32_t> standardized;
  webrtc::RTCNonStandardStatsMember<int32_t> non_standardized;
  webrtc::RTCStatsMember<std::string> foo_id;
  webrtc::RTCRestrictedStatsMember<
      bool,
      webrtc::StatExposureCriteria::kHardwareCapability>
      hw_stat;
};

WEBRTC_RTCSTATS_IMPL(TestStats,
                     webrtc::RTCStats,
                     "teststats",
                     &standardized,
                     &non_standardized,
                     &foo_id,
                     &hw_stat)

TestStats::TestStats(const std::string& id, webrtc::Timestamp timestamp)
    : RTCStats(id, timestamp),
      standardized("standardized"),
      non_standardized("non_standardized",
                       {webrtc::NonStandardGroupId::kGroupIdForTesting}),
      foo_id("fooId"),
      hw_stat("hwStat") {}

bool MemberIsDefined(const RTCStatsWrapper& stats, const char* name) {
  for (size_t i = 0; i < stats.MembersCount(); ++i) {
    std::unique_ptr<RTCStatsMember> member = stats.GetMember(i);
    if (member->GetName() == name && member->IsDefined()) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(RTCStatsTest, ReportSizeAndGetter) {
  const char* kFirstId = "FirstId";
  const char* kSecondId = "SecondId";

  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(42));
  webrtc_report->AddStats(std::make_unique<webrtc::RTCTestStats>(
      kFirstId, webrtc::Timestamp::Micros(42)));
  webrtc_report->AddStats(std::make_unique<webrtc::RTCTestStats>(
      kSecondId, webrtc::Timestamp::Micros(42)));

  RTCStatsReportPlatform report(webrtc_report.get(), {}, false);
  EXPECT_EQ(report.Size(), 2u);
  EXPECT_TRUE(report.GetStats(kFirstId));
  EXPECT_TRUE(report.GetStats(kSecondId));
}

TEST(RTCStatsTest, Iterator) {
  const char* kFirstId = "FirstId";
  const char* kSecondId = "SecondId";

  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(42));
  webrtc_report->AddStats(std::make_unique<webrtc::RTCTestStats>(
      kFirstId, webrtc::Timestamp::Micros(42)));
  webrtc_report->AddStats(std::make_unique<webrtc::RTCTestStats>(
      kSecondId, webrtc::Timestamp::Micros(42)));

  RTCStatsReportPlatform report(webrtc_report.get(), {}, false);
  EXPECT_EQ(report.Size(), 2u);

  std::unique_ptr<RTCStatsWrapper> stats = report.Next();
  EXPECT_TRUE(stats);
  EXPECT_EQ(stats->Id(), kFirstId);
  stats = report.Next();
  EXPECT_TRUE(stats);
  EXPECT_EQ(stats->Id(), kSecondId);
  EXPECT_FALSE(report.Next());
}

// Similar to how only allowlisted stats objects should be surfaced, only
// standardized members of the allowlisted objects should be surfaced.
TEST(RTCStatsTest, OnlyIncludeStandarizedMembers) {
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(42));
  webrtc_report->AddStats(
      std::make_unique<TestStats>("id", webrtc::Timestamp::Micros(0)));

  // TestStats has three members, but the non-standard member should be filtered
  // out.
  RTCStatsReportPlatform report(webrtc_report.get(), {}, false);
  std::unique_ptr<RTCStatsWrapper> stats = report.Next();
  ASSERT_NE(nullptr, stats);
  ASSERT_EQ(3u, stats->MembersCount());
  EXPECT_EQ("standardized", stats->GetMember(0)->GetName());
  EXPECT_EQ("fooId", stats->GetMember(1)->GetName());
}

TEST(RTCStatsTest, IncludeAllMembers) {
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(7));
  webrtc_report->AddStats(
      std::make_unique<TestStats>("id", webrtc::Timestamp::Micros(0)));

  // Include both standard and non-standard member.
  RTCStatsReportPlatform report(
      webrtc_report.get(),
      Vector<webrtc::NonStandardGroupId>{
          webrtc::NonStandardGroupId::kGroupIdForTesting},
      false);
  std::unique_ptr<RTCStatsWrapper> stats = report.GetStats("id");
  ASSERT_NE(nullptr, stats);
  ASSERT_EQ(4u, stats->MembersCount());
  EXPECT_EQ("standardized", stats->GetMember(0)->GetName());
  EXPECT_EQ("non_standardized", stats->GetMember(1)->GetName());
  EXPECT_EQ("fooId", stats->GetMember(2)->GetName());
}

TEST(RTCStatsTest, IncludeAllMembersFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kWebRtcExposeNonStandardStats);

  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(7));
  webrtc_report->AddStats(
      std::make_unique<TestStats>("id", webrtc::Timestamp::Micros(0)));

  // Include both standard and non-standard member.
  RTCStatsReportPlatform report(
      webrtc_report.get(),
      Vector<webrtc::NonStandardGroupId>{
          webrtc::NonStandardGroupId::kGroupIdForTesting},
      false);
  std::unique_ptr<RTCStatsWrapper> stats = report.GetStats("id");
  ASSERT_NE(nullptr, stats);
  ASSERT_EQ(4u, stats->MembersCount());
  EXPECT_EQ("standardized", stats->GetMember(0)->GetName());
  EXPECT_EQ("non_standardized", stats->GetMember(1)->GetName());
  EXPECT_EQ("fooId", stats->GetMember(2)->GetName());
}

TEST(RTCStatsTest, CopyHandle) {
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(17));
  webrtc_report->AddStats(
      std::make_unique<TestStats>("id", webrtc::Timestamp::Micros(0)));

  // Check that filtering options are preserved during copy.
  RTCStatsReportPlatform standard_members_report(webrtc_report.get(), {},
                                                 false);
  std::unique_ptr<RTCStatsReportPlatform> standard_members_copy =
      standard_members_report.CopyHandle();

  ASSERT_EQ(3u, standard_members_report.GetStats("id")->MembersCount());
  ASSERT_EQ(3u, standard_members_copy->GetStats("id")->MembersCount());

  RTCStatsReportPlatform all_members_report(
      webrtc_report.get(),
      Vector<webrtc::NonStandardGroupId>{
          webrtc::NonStandardGroupId::kGroupIdForTesting},
      false);
  std::unique_ptr<RTCStatsReportPlatform> all_members_copy =
      all_members_report.CopyHandle();
  ASSERT_EQ(4u, all_members_report.GetStats("id")->MembersCount());
  ASSERT_EQ(4u, all_members_copy->GetStats("id")->MembersCount());
}

TEST(RTCStatsTest, ExcludeDeprecated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(blink::WebRtcUnshipDeprecatedStats);

  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(1234));
  {
    auto stats_with_deprecated_foo_id = std::make_unique<TestStats>(
        "NotDeprecated_a", webrtc::Timestamp::Micros(1234));
    stats_with_deprecated_foo_id->foo_id = "DEPRECATED_b";
    webrtc_report->AddStats(std::move(stats_with_deprecated_foo_id));
  }
  webrtc_report->AddStats(std::make_unique<TestStats>(
      "DEPRECATED_b", webrtc::Timestamp::Micros(1234)));
  {
    auto stats_with_non_deprecated_foo_id = std::make_unique<TestStats>(
        "NotDeprecated_c", webrtc::Timestamp::Micros(1234));
    stats_with_non_deprecated_foo_id->foo_id = "NotDeprecated_a";
    webrtc_report->AddStats(std::move(stats_with_non_deprecated_foo_id));
  }

  RTCStatsReportPlatform report(webrtc_report.get(), {}, false);
  EXPECT_FALSE(report.GetStats("DEPRECATED_b"));
  EXPECT_EQ(report.Size(), 2u);
  EXPECT_TRUE(report.Next());
  EXPECT_TRUE(report.Next());
  EXPECT_FALSE(report.Next());

  auto stats_with_deprecated_foo_id = report.GetStats("NotDeprecated_a");
  ASSERT_TRUE(stats_with_deprecated_foo_id);
  // fooId is excluded because it is an "Id" member with a "DEPRECATED_"
  // reference.
  EXPECT_EQ(stats_with_deprecated_foo_id->MembersCount(), 2u);
  EXPECT_EQ(stats_with_deprecated_foo_id->GetMember(0)->GetName(),
            "standardized");

  auto stats_with_non_deprecated_foo_id = report.GetStats("NotDeprecated_c");
  ASSERT_TRUE(stats_with_deprecated_foo_id);
  // fooId is included, it's not referencing anything deprecated.
  EXPECT_EQ(stats_with_non_deprecated_foo_id->MembersCount(), 3u);
  EXPECT_EQ(stats_with_non_deprecated_foo_id->GetMember(0)->GetName(),
            "standardized");
  EXPECT_EQ(stats_with_non_deprecated_foo_id->GetMember(1)->GetName(), "fooId");
}

TEST(RTCStatsTest, FlagsControllingDeprecation) {
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(1234));
  auto webrtc_test_stats =
      std::make_unique<TestStats>("test", webrtc::Timestamp::Micros(1234));
  webrtc_test_stats->foo_id = "DEPRECATED_reference";
  webrtc_report->AddStats(std::move(webrtc_test_stats));

  // WebRtcUnshipDeprecatedStats is enabled-by-default.
  {
    RTCStatsReportPlatform report(
        webrtc_report.get(), {},
        /*is_track_stats_deprecation_trial_enabled=*/false);
    std::unique_ptr<RTCStatsWrapper> test_stats = report.GetStats("test");
    EXPECT_FALSE(MemberIsDefined(*test_stats, "fooId"));
  }
  // Deprecated stats are accessible when disabling WebRtcUnshipDeprecatedStats.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(blink::WebRtcUnshipDeprecatedStats);
    RTCStatsReportPlatform report(
        webrtc_report.get(), {},
        /*is_track_stats_deprecation_trial_enabled=*/false);
    std::unique_ptr<RTCStatsWrapper> test_stats = report.GetStats("test");
    EXPECT_TRUE(MemberIsDefined(*test_stats, "fooId"));
  }
  // Deprecated stats are accessible when
  // `is_track_stats_deprecation_trial_enabled` is true.
  {
    RTCStatsReportPlatform report(
        webrtc_report.get(), {},
        /*is_track_stats_deprecation_trial_enabled=*/true);
    std::unique_ptr<RTCStatsWrapper> test_stats = report.GetStats("test");
    EXPECT_TRUE(MemberIsDefined(*test_stats, "fooId"));
  }
}

TEST(RTCStatsTest, StatsExposingHardwareCapabilitiesAreMarked) {
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(1234));

  auto stats = std::make_unique<TestStats>("id", webrtc::Timestamp::Micros(0));
  stats->hw_stat = true;
  webrtc_report->AddStats(std::move(stats));

  RTCStatsReportPlatform report(webrtc_report.get(), {}, false);
  auto stats_from_report = report.GetStats("id");
  ASSERT_TRUE(stats_from_report);
  EXPECT_EQ(stats_from_report->MembersCount(), 3u);
  EXPECT_EQ(stats_from_report->GetMember(2)->Restriction(),
            RTCStatsMember::ExposureRestriction::kHardwareCapability);
}

}  // namespace blink
