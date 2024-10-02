// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_mediator.h"

#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_view_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Mock of ParcelTrackingSettingsModelConsumer.
@interface MockParcelTrackingSettingsModelConsumer
    : NSObject <ParcelTrackingSettingsModelConsumer>
@property(nonatomic, assign) IOSParcelTrackingOptInStatus latestStatus;
@end
@implementation MockParcelTrackingSettingsModelConsumer
- (void)updateCheckedState:(IOSParcelTrackingOptInStatus)newState {
  self.latestStatus = newState;
}
@end

// Tests the ParcelTrackingSettingsMediator's core functionality.
class ParcelTrackingSettingsMediatorUnittest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    consumer_ = [[MockParcelTrackingSettingsModelConsumer alloc] init];
    mediator_ = [[ParcelTrackingSettingsMediator alloc]
        initWithPrefs:profile_->GetTestingPrefService()];
  }

 protected:
  // Needed for test profile created by TestProfileIOS().
  web::WebTaskEnvironment task_environment_;

  std::unique_ptr<TestProfileIOS> profile_;
  ParcelTrackingSettingsMediator* mediator_;
  MockParcelTrackingSettingsModelConsumer* consumer_;
};

// Tests that the mediator passes the correct default opt-in status to the
// consumer upon initial configuration.
TEST_F(ParcelTrackingSettingsMediatorUnittest, TestLoadModel) {
  mediator_.consumer = consumer_;

  EXPECT_EQ(consumer_.latestStatus,
            IOSParcelTrackingOptInStatus::kStatusNotSet);

  [mediator_ disconnect];
}

// Tests that calling -tableViewDidSelectStatus: updates the backed pref
// correctly.
TEST_F(ParcelTrackingSettingsMediatorUnittest, TestDidSelectItemAtIndexPath) {
  [mediator_
      tableViewDidSelectStatus:IOSParcelTrackingOptInStatus::kNeverTrack];

  IOSParcelTrackingOptInStatus optInStatus =
      static_cast<IOSParcelTrackingOptInStatus>(
          profile_->GetTestingPrefService()->GetInteger(
              prefs::kIosParcelTrackingOptInStatus));
  EXPECT_EQ(optInStatus, IOSParcelTrackingOptInStatus::kNeverTrack);

  [mediator_ disconnect];
}
