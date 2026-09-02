// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using send_tab_to_self::FakeSendTabToSelfModel;
using send_tab_to_self::SendTabToSelfEntry;

namespace {

constexpr char kExampleURL[] = "https://www.example.com/";
constexpr char kLocalDeviceCacheGuid[] = "guid";

class SendTabToSelfTabCardLabelDataTest : public PlatformTest {
 public:
  SendTabToSelfTabCardLabelDataTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));

    profile_ = std::move(test_profile_builder).Build();
    model_ = static_cast<FakeSendTabToSelfModel*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get())
            ->GetSendTabToSelfModel());
    model_->SetLocalCacheGuid(kLocalDeviceCacheGuid);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_{
      send_tab_to_self::kSendTabToSelfAutoOpen};
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<FakeSendTabToSelfModel> model_;
};

// Tests that GetLabelTextForWebState returns nil when there is no matching
// entry in the model.
TEST_F(SendTabToSelfTabCardLabelDataTest, NoMatchingEntryInModel) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());
  web_state.SetCurrentURL(GURL(kExampleURL));
  web_state.SetLastActiveTime(base::Time::Now());

  NSString* label_text =
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state);
  EXPECT_EQ(label_text, nil);
}

// Tests that GetLabelTextForWebState returns valid label data when there is a
// matching entry in the model and the tab has not been viewed (last active
// time is equal to the opened time).
TEST_F(SendTabToSelfTabCardLabelDataTest, MatchingEntryUnviewed) {
  GURL url(kExampleURL);
  base::Time now = base::Time::Now();

  // Add the entry to the fake sync model.
  const SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      url, "Title", /*target_device_cache_guid=*/kLocalDeviceCacheGuid,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  model_->MarkEntryOpened(entry->GetGUID());

  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());
  web_state.SetCurrentURL(url);
  // Set the tab's active time to the same time it was marked as opened.
  web_state.SetLastActiveTime(now);

  NSString* label_text =
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state);

  NSString* expected_text = l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE, u"remote_device");
  EXPECT_NSEQ(label_text, expected_text);

  // Verify it also attached the UserData since the WebState is realized.
  EXPECT_NE(SendTabToSelfTabCardLabelData::FromWebState(&web_state), nullptr);
}

// Tests that GetLabelTextForWebState returns nil when the tab has been
// viewed (its last active time is newer than the opened time + 5s).
TEST_F(SendTabToSelfTabCardLabelDataTest, MatchingEntryViewed) {
  GURL url(kExampleURL);
  base::Time now = base::Time::Now();

  // Add the entry and mark it opened at 'now'.
  const SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      url, "Title", /*target_device_cache_guid=*/kLocalDeviceCacheGuid,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  model_->MarkEntryOpened(entry->GetGUID());

  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());
  web_state.SetCurrentURL(url);
  // Set the tab's active time to 10 seconds after it was opened, simulating
  // that the user viewed the tab.
  web_state.SetLastActiveTime(now + base::Seconds(10));

  NSString* label_text =
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state);
  EXPECT_EQ(label_text, nil);
}

// Tests that the label data is successfully attached and is cleared
// automatically when the tab is shown.
TEST_F(SendTabToSelfTabCardLabelDataTest, WasShownClearsLabel) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());

  // No label data should be attached initially.
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(&web_state, "test_guid",
                                                   "remote_device");

  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));
  EXPECT_NSEQ(
      @"From remote_device",
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state));

  // Simulating viewing the tab should clear the label and log activation.
  EXPECT_EQ(model_->last_activated_guid(), "");
  web_state.WasShown();
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));
  EXPECT_EQ(model_->last_activated_guid(), "test_guid");
  EXPECT_EQ(model_->last_activated_entry_point(),
            send_tab_to_self::ShareActivatedEntryPoint::kTabStrip);
}

// Tests that the label data is successfully cleaned up when the WebState is
// destroyed without ever being shown. Also crucial for verifying that raw
// destruction (which occurs during browser shutdown) does NOT log any
// activation or abandonment metrics.
TEST_F(SendTabToSelfTabCardLabelDataTest, WebStateDestroyedClearsLabel) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(
      web_state.get(), "destroy_guid", "remote_device");

  // Verify it is attached.
  EXPECT_NE(nullptr,
            SendTabToSelfTabCardLabelData::FromWebState(web_state.get()));

  // Destroy the WebState. This will trigger WebStateDestroyed, removing the
  // observer and safely destructing the label data.
  web_state.reset();
  // Verify that no metrics were logged. This ensures we don't log abandonment
  // during browser shutdown or tab strip destruction.
  EXPECT_EQ(model_->last_activated_guid(), "");
}

// Tests that the activation metric is logged even if the model is not ready
// when the tab is shown.
TEST_F(SendTabToSelfTabCardLabelDataTest, WasShownLogsMetricWhenModelNotReady) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(&web_state, "test_guid",
                                                   "remote_device");

  // Set the model to not ready.
  model_->SetIsReady(false);

  // Simulating viewing the tab should clear the label and log activation
  // even if the model is not ready.
  EXPECT_EQ(model_->last_activated_guid(), "");
  web_state.WasShown();
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));
  EXPECT_EQ(model_->last_activated_guid(), "test_guid");
  EXPECT_EQ(model_->last_activated_entry_point(),
            send_tab_to_self::ShareActivatedEntryPoint::kTabStrip);
}

// Tests that WebStateClosedByUser logs the abandonment metric.
TEST_F(SendTabToSelfTabCardLabelDataTest, WebStateClosedByUserLogsMetric) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(&web_state, "test_guid",
                                                   "remote_device");

  EXPECT_EQ(model_->last_activated_guid(), "");

  SendTabToSelfTabCardLabelData* label_data =
      SendTabToSelfTabCardLabelData::FromWebState(&web_state);
  ASSERT_NE(nullptr, label_data);

  // Simulating the user closing the tab should log abandonment.
  label_data->WebStateClosedByUser(&web_state);

  EXPECT_EQ(model_->last_activated_guid(), "test_guid");
  EXPECT_EQ(model_->last_activated_entry_point(),
            send_tab_to_self::ShareActivatedEntryPoint::
                kTabOrBrowserClosedWithoutActivation);
}

// Tests that the label data automatically expires after 5 days.
TEST_F(SendTabToSelfTabCardLabelDataTest, ExpiryClearsLabel) {
  web::FakeWebState web_state;

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(&web_state, "expiry_guid",
                                                   "remote_device");

  SendTabToSelfTabCardLabelData* label_data =
      SendTabToSelfTabCardLabelData::FromWebState(&web_state);
  ASSERT_NE(nullptr, label_data);

  // Fast forward 4 days (less than 5 days). The label should still be valid.
  task_environment_.FastForwardBy(base::Days(4));
  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));

  // Fast forward another 2 days (total 6 days). The label should now be
  // expired and cleared.
  task_environment_.FastForwardBy(base::Days(2));
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));
}

// Tests that GetLabelTextForWebState returns nil when the feature flag is
// disabled.
TEST_F(SendTabToSelfTabCardLabelDataTest, FeatureDisabledReturnsNull) {
  // Re-initialize the feature list to be disabled for this test.
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      send_tab_to_self::kSendTabToSelfAutoOpen);

  GURL url(kExampleURL);
  base::Time now = base::Time::Now();

  // Add an unviewed matching entry.
  const SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      url, "Title", /*target_device_cache_guid=*/kLocalDeviceCacheGuid,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  model_->MarkEntryOpened(entry->GetGUID());

  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());
  web_state.SetCurrentURL(url);
  web_state.SetLastActiveTime(now);

  NSString* label_text =
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state);

  // The label should NOT be created because the feature flag is disabled.
  EXPECT_EQ(label_text, nil);
}

// Tests that after a label is shown (dismissed), it is not recreated even if
// GetLabelTextForWebState is called again within the active 5-second window.
TEST_F(SendTabToSelfTabCardLabelDataTest, DismissedLabelIsNotRecreated) {
  GURL url(kExampleURL);
  base::Time now = base::Time::Now();

  // Add the entry and mark it opened at 'now'.
  const SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      url, "Title", /*target_device_cache_guid=*/kLocalDeviceCacheGuid,
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  model_->MarkEntryOpened(entry->GetGUID());

  web::FakeWebState web_state;
  web_state.SetBrowserState(profile_.get());
  web_state.SetCurrentURL(url);
  // Initially, the tab's active time is equal to the opened time (unviewed).
  web_state.SetLastActiveTime(now);

  // Create the label.
  NSString* label_text =
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state);
  ASSERT_NE(label_text, nil);

  // Simulate viewing the tab by triggering WasShown(). This should immediately
  // delete the label.
  web_state.WasShown();

  // Verify that the label data has been removed.
  EXPECT_EQ(SendTabToSelfTabCardLabelData::FromWebState(&web_state), nullptr);

  // Set the tab's active time to be within the 5s window of the opened time
  // (e.g. now + 2 seconds).
  web_state.SetLastActiveTime(now + base::Seconds(2));

  // Call GetLabelTextForWebState again. It should NOT recreate the label
  // because it was already viewed (last active time no longer equals creation
  // time).
  NSString* recreated_label_text =
      SendTabToSelfTabCardLabelData::GetLabelTextForWebState(&web_state);
  EXPECT_EQ(recreated_label_text, nil);
}

}  // namespace
