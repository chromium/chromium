// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/utils/utils.h"

#import <optional>
#import <string>

#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_util.h"
#import "components/sync_device_info/fake_device_info_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"
#import "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Test implementation of `CrossDevicePrefTracker`.
class TestCrossDevicePrefTracker
    : public sync_preferences::CrossDevicePrefTracker {
 public:
  TestCrossDevicePrefTracker() = default;
  ~TestCrossDevicePrefTracker() override = default;

  // `KeyedService` overrides.
  void Shutdown() override {}

  // `CrossDevicePrefTracker` overrides.
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  std::vector<sync_preferences::TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    auto it = pref_values_.find(pref_name);
    if (it == pref_values_.end()) {
      return {};
    }

    std::vector<sync_preferences::TimestampedPrefValue> result;
    for (const auto& timestamped_value : it->second) {
      sync_preferences::TimestampedPrefValue copied_value =
          timestamped_value.Clone();
      result.push_back(std::move(copied_value));
    }
    return result;
  }

  std::optional<sync_preferences::TimestampedPrefValue> GetMostRecentValue(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    return std::nullopt;
  }

  // Testing Method for injecting pref values into the tracker.
  void AddSyncedPrefValue(std::string_view pref_name,
                          sync_preferences::TimestampedPrefValue& value) {
    pref_values_[pref_name].push_back(std::move(value));
  }

 private:
  // Testing member. Map containing TimestampedPrefValues mapped to their
  // associated pref's name.
  std::map<std::string_view,
           std::vector<sync_preferences::TimestampedPrefValue>>
      pref_values_;
};

}  // namespace

// Test suite for Synced Set Up utility functions.
class SyncedSetUpUtilsTest : public PlatformTest {
 public:
  void InitializeProfileState() {
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    ASSERT_NSEQ(profile_state_.foregroundActiveScene, nil);
  }

  // Creates a DeviceInfo object.
  std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfoForTesting(
      std::string guid,
      syncer::DeviceInfo::FormFactor form_factor,
      syncer::DeviceInfo::OsType os_type,
      base::Time last_updated_timestamp = base::Time::Now()) {
    return CreateFakeDeviceInfo(guid, "Device Name", std::nullopt,
                                sync_pb::SyncEnums::TYPE_UNSET, os_type,
                                form_factor, "manufacturer", "model",
                                std::string(), last_updated_timestamp);
  }

  // Helper for creating a DeviceInfo object.
  std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
      const std::string& guid,
      const std::string& name = "name",
      const std::optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
          std::nullopt,
      sync_pb::SyncEnums_DeviceType device_type =
          sync_pb::SyncEnums_DeviceType_TYPE_UNSET,
      syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kUnknown,
      syncer::DeviceInfo::FormFactor form_factor =
          syncer::DeviceInfo::FormFactor::kUnknown,
      const std::string& manufacturer_name = "manufacturer",
      const std::string& model_name = "model",
      const std::string& full_hardware_class = std::string(),
      base::Time last_updated_timestamp = base::Time::Now()) {
    return std::make_unique<syncer::DeviceInfo>(
        guid, name, "chrome_version", "user_agent", device_type, os_type,
        form_factor, "device_id", manufacturer_name, model_name,
        full_hardware_class, last_updated_timestamp,
        syncer::DeviceInfoUtil::GetPulseInterval(),
        /*send_tab_to_self_receiving_enabled=*/
        false,
        sync_pb::
            SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
        sharing_info,
        /*paask_info=*/std::nullopt,
        /*fcm_registration_token=*/std::string(),
        /*interested_data_types=*/syncer::DataTypeSet(),
        /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
  }

  // Helper for configuring a TimestampedPrefValue.
  void ConfigureTimestampedPrefValue(
      sync_preferences::TimestampedPrefValue& timestamped_value,
      base::Value value,
      std::string device_sync_cache_guid,
      base::Time last_observed_change_time = base::Time::Now()) {
    timestamped_value.value = value.Clone();
    timestamped_value.last_observed_change_time = last_observed_change_time;
    timestamped_value.device_sync_cache_guid = device_sync_cache_guid;
  }

  // Helper function to create and connect a scene with a specific activation
  // level.
  SceneState* ConnectSceneWithActivationLevel(SceneActivationLevel level) {
    SceneState* scene = [[SceneState alloc] initWithAppState:nil];
    scene.activationLevel = level;

    [profile_state_ sceneStateConnected:scene];

    if (level == SceneActivationLevelForegroundActive) {
      EXPECT_EQ(profile_state_.foregroundActiveScene, scene);
    } else {
      EXPECT_EQ(profile_state_.foregroundActiveScene, nil);
    }

    return scene;
  }

 protected:
  ProfileState* profile_state_;
  base::test::TaskEnvironment task_environment_;
  TestCrossDevicePrefTracker pref_tracker_;
  syncer::FakeDeviceInfoTracker device_info_tracker_;
};

// Test that a device with a matching form factor is chosen as the best fit
// device.
TEST_F(SyncedSetUpUtilsTest, TestMatchPrefsByFormFactor) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device = CreateDeviceInfoForTesting(
      "local_device", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // Android tablet.
  std::unique_ptr<syncer::DeviceInfo> android_tablet =
      CreateDeviceInfoForTesting("android_tablet",
                                 syncer::DeviceInfo::FormFactor::kTablet,
                                 syncer::DeviceInfo::OsType::kAndroid);

  // Android phone (match).
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      CreateDeviceInfoForTesting("android_phone",
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kAndroid);

  // iOS tablet.
  std::unique_ptr<syncer::DeviceInfo> ios_tablet = CreateDeviceInfoForTesting(
      "ios_tablet", syncer::DeviceInfo::FormFactor::kTablet,
      syncer::DeviceInfo::OsType::kIOS);

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_tablet.get());
  device_info_tracker_.Add(android_phone.get());
  device_info_tracker_.Add(ios_tablet.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  sync_preferences::TimestampedPrefValue local_device_magic_stack_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);

  sync_preferences::TimestampedPrefValue android_tablet_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue android_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_tablet_magic_stack_enabled,
                                base::Value(true),
                                android_tablet.get()->guid());
  ConfigureTimestampedPrefValue(android_tablet_most_visited_enabled,
                                base::Value(false),
                                android_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_tablet_most_visited_enabled);

  sync_preferences::TimestampedPrefValue android_phone_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue android_phone_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_phone_magic_stack_enabled,
                                base::Value(false),
                                android_phone.get()->guid());
  ConfigureTimestampedPrefValue(android_phone_most_visited_enabled,
                                base::Value(true), android_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_phone_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_phone_most_visited_enabled);

  sync_preferences::TimestampedPrefValue ios_tablet_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue ios_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_tablet_magic_stack_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  ConfigureTimestampedPrefValue(ios_tablet_most_visited_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_tablet_most_visited_enabled);

  // Expect that the prefs from the Android phone are returned.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          android_phone_magic_stack_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
                          android_phone_most_visited_enabled.value.Clone()});

  std::map<std::string_view, base::Value> result =
      GetCrossDevicePrefsFromRemoteDevice(&pref_tracker_, &device_info_tracker_,
                                          local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // Compare the resultant map to the expected map.
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Test that a device with a matching OS is chosen as the best fit device if
// there is no device with a matching form factor.
TEST_F(SyncedSetUpUtilsTest, TestMatchPrefsByOsType) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device = CreateDeviceInfoForTesting(
      "local_device", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // Android tablet.
  std::unique_ptr<syncer::DeviceInfo> android_tablet =
      CreateDeviceInfoForTesting("android_tablet",
                                 syncer::DeviceInfo::FormFactor::kTablet,
                                 syncer::DeviceInfo::OsType::kAndroid);

  // iOS tablet (match).
  std::unique_ptr<syncer::DeviceInfo> ios_tablet = CreateDeviceInfoForTesting(
      "ios_tablet", syncer::DeviceInfo::FormFactor::kTablet,
      syncer::DeviceInfo::OsType::kIOS);

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_tablet.get());
  device_info_tracker_.Add(ios_tablet.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  sync_preferences::TimestampedPrefValue local_device_magic_stack_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);

  sync_preferences::TimestampedPrefValue android_tablet_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue android_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_tablet_magic_stack_enabled,
                                base::Value(true),
                                android_tablet.get()->guid());
  ConfigureTimestampedPrefValue(android_tablet_most_visited_enabled,
                                base::Value(false),
                                android_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_tablet_most_visited_enabled);

  sync_preferences::TimestampedPrefValue ios_tablet_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue ios_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_tablet_magic_stack_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  ConfigureTimestampedPrefValue(ios_tablet_most_visited_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_tablet_most_visited_enabled);

  // Expect that the prefs from the iOS tablet are returned.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          ios_tablet_magic_stack_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
                          ios_tablet_most_visited_enabled.value.Clone()});

  std::map<std::string_view, base::Value> result =
      GetCrossDevicePrefsFromRemoteDevice(&pref_tracker_, &device_info_tracker_,
                                          local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // compare the returned map to the expected map
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Test that a device with the highest volume of observed pref changes is chosen
// as the best fit device if the synced devices score the same against the
// current device on form factor and OS.
TEST_F(SyncedSetUpUtilsTest, TestMatchPrefsByObservedChangeCount) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device = CreateDeviceInfoForTesting(
      "local_device", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // Android phone.
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      CreateDeviceInfoForTesting("android_phone",
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kAndroid);

  // iOS phone.
  std::unique_ptr<syncer::DeviceInfo> ios_phone = CreateDeviceInfoForTesting(
      "ios_phone", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // iOS phone (match).
  std::unique_ptr<syncer::DeviceInfo> ios_phone_2 = CreateDeviceInfoForTesting(
      "ios_phone_2", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_phone.get());
  device_info_tracker_.Add(ios_phone.get());
  device_info_tracker_.Add(ios_phone_2.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  sync_preferences::TimestampedPrefValue local_device_magic_stack_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);

  sync_preferences::TimestampedPrefValue android_phone_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue android_phone_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_phone_magic_stack_enabled,
                                base::Value(true), android_phone.get()->guid());
  ConfigureTimestampedPrefValue(android_phone_most_visited_enabled,
                                base::Value(false),
                                android_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_phone_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_phone_most_visited_enabled);

  sync_preferences::TimestampedPrefValue ios_phone_1_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue ios_phone_1_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_phone_1_magic_stack_enabled,
                                base::Value(false), ios_phone.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_1_most_visited_enabled,
                                base::Value(false), ios_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_1_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_1_most_visited_enabled);

  sync_preferences::TimestampedPrefValue ios_phone_2_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue ios_phone_2_most_visited_enabled;
  sync_preferences::TimestampedPrefValue ios_phone_2_price_tracking_enabled;
  ConfigureTimestampedPrefValue(ios_phone_2_magic_stack_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_most_visited_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_price_tracking_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_2_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_2_most_visited_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      ios_phone_2_price_tracking_enabled);

  // Expect that the prefs from the second iOS phone with more changes are
  // returned.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          ios_phone_2_magic_stack_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
                          ios_phone_2_most_visited_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
                          ios_phone_2_price_tracking_enabled.value.Clone()});

  std::map<std::string_view, base::Value> result =
      GetCrossDevicePrefsFromRemoteDevice(&pref_tracker_, &device_info_tracker_,
                                          local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // Compare the resultant map to the expected map.
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Tests that no new prefs to apply are returned if the local device has a
// higher volume of observed pref changes than the otherwise best fitting
// device.
TEST_F(SyncedSetUpUtilsTest, TestKeepLocalPrefsByChangeActivity) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device = CreateDeviceInfoForTesting(
      "local_device", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // Android phone.
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      CreateDeviceInfoForTesting("android_phone",
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kAndroid);

  // iOS phone.
  std::unique_ptr<syncer::DeviceInfo> ios_phone = CreateDeviceInfoForTesting(
      "ios_phone", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // iOS phone (match).
  std::unique_ptr<syncer::DeviceInfo> ios_phone_2 = CreateDeviceInfoForTesting(
      "ios_phone_2", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_phone.get());
  device_info_tracker_.Add(ios_phone.get());
  device_info_tracker_.Add(ios_phone_2.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  sync_preferences::TimestampedPrefValue local_device_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue local_device_most_visited_enabled;
  sync_preferences::TimestampedPrefValue local_device_price_tracking_enabled;
  sync_preferences::TimestampedPrefValue local_device_safety_check_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  ConfigureTimestampedPrefValue(local_device_most_visited_enabled,
                                base::Value(true), local_device.get()->guid());
  ConfigureTimestampedPrefValue(local_device_price_tracking_enabled,
                                base::Value(true), local_device.get()->guid());
  ConfigureTimestampedPrefValue(local_device_safety_check_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      local_device_most_visited_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      local_device_price_tracking_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceSafetyCheckHomeModuleEnabled,
      local_device_safety_check_enabled);

  sync_preferences::TimestampedPrefValue android_phone_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue android_phone_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_phone_magic_stack_enabled,
                                base::Value(true), android_phone.get()->guid());
  ConfigureTimestampedPrefValue(android_phone_most_visited_enabled,
                                base::Value(false),
                                android_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_phone_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_phone_most_visited_enabled);

  sync_preferences::TimestampedPrefValue ios_phone_1_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue ios_phone_1_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_phone_1_magic_stack_enabled,
                                base::Value(false), ios_phone.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_1_most_visited_enabled,
                                base::Value(false), ios_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_1_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_1_most_visited_enabled);

  sync_preferences::TimestampedPrefValue ios_phone_2_magic_stack_enabled;
  sync_preferences::TimestampedPrefValue ios_phone_2_most_visited_enabled;
  sync_preferences::TimestampedPrefValue ios_phone_2_price_tracking_enabled;
  ConfigureTimestampedPrefValue(ios_phone_2_magic_stack_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_most_visited_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_price_tracking_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_2_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_2_most_visited_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      ios_phone_2_price_tracking_enabled);

  // Expect that no new prefs are returned.
  std::map<std::string_view, base::Value> result =
      GetCrossDevicePrefsFromRemoteDevice(&pref_tracker_, &device_info_tracker_,
                                          local_device.get());
  EXPECT_TRUE(result.empty());
}

// Tests that if a pref has multiple observed changes only the most recently
// observed pref change is retrieved.
TEST_F(SyncedSetUpUtilsTest, TestReturnsMostRecentObservedPrefChanges) {
  const base::Time kNow = base::Time::Now();

  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device = CreateDeviceInfoForTesting(
      "local_device", syncer::DeviceInfo::FormFactor::kPhone,
      syncer::DeviceInfo::OsType::kIOS);

  // Android phone (match).
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      CreateDeviceInfoForTesting("android_phone",
                                 syncer::DeviceInfo::FormFactor::kPhone,
                                 syncer::DeviceInfo::OsType::kAndroid);

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_phone.get());

  // Configure some `TimestampedPrefValue` objects for the same pref associated
  // with the Android phone GUID's and add them to the pref tracker. These
  // represent the same pref being changed several times over a period of time.
  sync_preferences::TimestampedPrefValue magic_stack_enabled_day;
  sync_preferences::TimestampedPrefValue magic_stack_enabled_now;
  sync_preferences::TimestampedPrefValue magic_stack_enabled_week;
  ConfigureTimestampedPrefValue(magic_stack_enabled_day, base::Value(true),
                                android_phone.get()->guid(),
                                kNow - base::Days(1));
  ConfigureTimestampedPrefValue(magic_stack_enabled_now, base::Value(false),
                                android_phone.get()->guid(), kNow);
  ConfigureTimestampedPrefValue(magic_stack_enabled_week, base::Value(true),
                                android_phone.get()->guid(),
                                kNow - base::Days(7));
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled, magic_stack_enabled_day);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled, magic_stack_enabled_now);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled, magic_stack_enabled_week);

  // Expect that the pref is returned with its most recently set value.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          magic_stack_enabled_now.value.Clone()});

  std::map<std::string_view, base::Value> result =
      GetCrossDevicePrefsFromRemoteDevice(&pref_tracker_, &device_info_tracker_,
                                          local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // Compare the resultant map to the expected map.
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Tests that the active scene is returned when all preconditions are met (main
// provider active, profile ready, no blockers).
TEST_F(SyncedSetUpUtilsTest, ReturnsActiveSceneWhenAllPreconditionsMet) {
  InitializeProfileState();
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  SceneState* scene =
      ConnectSceneWithActivationLevel(SceneActivationLevelForegroundActive);

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, scene);
}

// Tests that `nil` is returned if the input `ProfileState` is `null`.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenProfileStateIsNull) {
  InitializeProfileState();
  SceneState* result = GetEligibleSceneForSyncedSetUp(nil);

  EXPECT_EQ(result, nil);
}

// Tests that `nil` is returned if the profile initialization is not complete.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenProfileIsNotFinalized) {
  InitializeProfileState();
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kProfileLoaded);

  ConnectSceneWithActivationLevel(SceneActivationLevelForegroundActive);

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, nil);
}

// Tests that `nil` is returned if a UI blocker is present.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenUIBlockerIsPresent) {
  InitializeProfileState();
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  SceneState* scene =
      ConnectSceneWithActivationLevel(SceneActivationLevelForegroundActive);

  [profile_state_ incrementBlockingUICounterForTarget:scene];

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, nil);
}

// Tests that `nil` is returned if there is no foreground active scene.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenNoForegroundActiveSceneExists) {
  InitializeProfileState();
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  ConnectSceneWithActivationLevel(SceneActivationLevelForegroundInactive);

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, nil);
}
