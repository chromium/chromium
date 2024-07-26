// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_mediator.h"

#import "base/files/file.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using startup_metric_utils::FirstRunSentinelCreationResult;

// Tests the MagicStackHalfSheetMediator functionality.
class MagicStackHalfSheetMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kSafetyCheckMagicStack, kTabResumption}, {});

    // Necessary set up for kIOSSetUpList.
    local_state()->ClearPref(set_up_list_prefs::kDisabled);
    ClearDefaultBrowserPromoData();

    consumer_ = OCMStrictProtocolMock(@protocol(MagicStackHalfSheetConsumer));
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  // Clears and re-writes the FirstRun sentinel file, in order to allow Set Up
  // List to display.
  void WriteFirstRunSentinel() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FirstRun::RemoveSentinel();
    base::File::Error file_error = base::File::FILE_OK;
    FirstRunSentinelCreationResult sentinel_created =
        FirstRun::CreateSentinel(&file_error);
    ASSERT_EQ(sentinel_created, FirstRunSentinelCreationResult::kSuccess)
        << "Error creating FirstRun sentinel: "
        << base::File::ErrorToString(file_error);
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
    EXPECT_FALSE(set_up_list_prefs::IsSetUpListDisabled(local_state()));
    EXPECT_FALSE(FirstRun::IsChromeFirstRun());
    EXPECT_TRUE(set_up_list_utils::IsSetUpListActive(local_state()));
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  MagicStackHalfSheetMediator* mediator_;
  id consumer_;
};

// Tests that the mediator makes the proper consumer calls.
TEST_F(MagicStackHalfSheetMediatorTest, TestConsumer) {
  // Removes the First Run Sentinel in case it is set.
  if (FirstRun::RemoveSentinel()) {
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
    FirstRun::IsChromeFirstRun();
  }

  // Set Up List should not be shown.
  mediator_ =
      [[MagicStackHalfSheetMediator alloc] initWithPrefService:local_state()];
  OCMExpect([consumer_ setSafetyCheckDisabled:NO]);
  OCMExpect([consumer_ setTabResumptionDisabled:NO]);

  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);

  WriteFirstRunSentinel();
  mediator_ =
      [[MagicStackHalfSheetMediator alloc] initWithPrefService:local_state()];

  OCMExpect([consumer_ showSetUpList:YES]);
  OCMExpect([consumer_ setSetUpListDisabled:NO]);
  OCMExpect([consumer_ setSafetyCheckDisabled:NO]);
  OCMExpect([consumer_ setTabResumptionDisabled:NO]);

  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Parcel tracking should only be shown in the US.
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");

  mediator_ =
      [[MagicStackHalfSheetMediator alloc] initWithPrefService:local_state()];

  OCMExpect([consumer_ showSetUpList:YES]);
  OCMExpect([consumer_ setSetUpListDisabled:NO]);
  OCMExpect([consumer_ setSafetyCheckDisabled:NO]);
  OCMExpect([consumer_ setTabResumptionDisabled:NO]);
  OCMExpect([consumer_ setParcelTrackingDisabled:NO]);

  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}
