// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios.h"

#import <memory>
#import <optional>
#import <string>

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/protobuf_matchers.h"
#import "base/test/task_environment.h"
#import "components/sync/base/client_tag_hash.h"
#import "components/sync/base/data_type.h"
#import "components/sync/model/model_error.h"
#import "components/sync/model/sync_change.h"
#import "components/sync/model/sync_change_processor.h"
#import "components/sync/model/sync_data.h"
#import "components/sync/protocol/entity_data.h"
#import "components/sync/protocol/entity_specifics.pb.h"
#import "components/sync/protocol/theme_ios_specifics.pb.h"
#import "components/sync/test/fake_sync_change_processor.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// A mock of `ThemeSyncableServiceIOS::Delegate`.
class MockThemeSyncDelegate : public ThemeSyncableServiceIOS::Delegate {
 public:
  MOCK_METHOD(sync_pb::ThemeIosSpecifics,
              GetCurrentTheme,
              (),
              (const, override));
  MOCK_METHOD(void,
              ApplyTheme,
              (const sync_pb::ThemeIosSpecifics&),
              (override));
  MOCK_METHOD(void, CacheLocalTheme, (), (override));
  MOCK_METHOD(void, RestoreCachedTheme, (), (override));
  MOCK_METHOD(bool, IsCurrentThemeSyncable, (), (const, override));
};

// Creates a `ThemeIosSpecifics` with the specified user color.
sync_pb::ThemeIosSpecifics CreateTestTheme(uint32_t color) {
  sync_pb::ThemeIosSpecifics theme;
  theme.mutable_user_color_theme()->set_color(color);
  return theme;
}

// Wraps a newly created test theme in a `syncer::SyncData` object.
syncer::SyncData CreateSyncData(uint32_t color,
                                const std::string& hash = "current_theme_ios") {
  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_theme_ios() = CreateTestTheme(color);
  return syncer::SyncData::CreateRemoteData(
      specifics, syncer::ClientTagHash::FromUnhashed(syncer::THEMES_IOS, hash));
}

// Creates a `syncer::SyncChange` of the specified type for a test theme.
syncer::SyncChange CreateSyncChange(syncer::SyncChange::SyncChangeType type,
                                    uint32_t color) {
  return syncer::SyncChange(FROM_HERE, type, CreateSyncData(color));
}

// Test fixture for the ThemeSyncableServiceIOS.
class ThemeSyncableServiceIOSTest : public PlatformTest {
 public:
  ThemeSyncableServiceIOSTest() {
    service_ = std::make_unique<ThemeSyncableServiceIOS>(&delegate_);
  }

  void SetUp() override {
    ON_CALL(delegate_, IsCurrentThemeSyncable()).WillByDefault(Return(true));
  }

 protected:
  // Starts syncing and returns the fake change processor for inspection.
  syncer::FakeSyncChangeProcessor* StartSyncing(
      const syncer::SyncDataList& data = {}) {
    auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
    auto* processor_ptr = processor.get();

    std::optional<syncer::ModelError> error =
        service_->MergeDataAndStartSyncing(syncer::THEMES_IOS, data,
                                           std::move(processor));
    EXPECT_FALSE(error.has_value());
    return processor_ptr;
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockThemeSyncDelegate> delegate_;
  std::unique_ptr<ThemeSyncableServiceIOS> service_;
};

#pragma mark - Lifecycle Tests

// Verifies that the service returns the correct client tag for iOS themes.
TEST_F(ThemeSyncableServiceIOSTest, ReturnsCorrectClientTag) {
  syncer::EntityData data;
  data.specifics.mutable_theme_ios();

  EXPECT_EQ("current_theme_ios", service_->GetClientTag(data));
}

// Ensures the WaitUntilReadyToSync method immediately runs the provided
// closure.
TEST_F(ThemeSyncableServiceIOSTest, WaitUntilReadyToSyncRunsClosure) {
  base::RunLoop run_loop;
  service_->WaitUntilReadyToSync(run_loop.QuitClosure());
  run_loop.Run();
}

// Checks that the local theme is cached when initial sync starts.
TEST_F(ThemeSyncableServiceIOSTest, CachesLocalThemeOnInitialSync) {
  EXPECT_CALL(delegate_, CacheLocalTheme()).Times(1);
  service_->WillStartInitialSync();
}

#pragma mark - MergeDataAndStartSyncing Tests

// Verifies syncing starts successfully with an empty initial data set.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataWithNoDataSucceeds) {
  auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
  auto* processor_ptr = processor.get();

  std::optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, {}, std::move(processor));

  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(processor_ptr->changes().empty());
}

// Verifies syncing starts successfully with a single initial theme item.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataWithOneItemSucceeds) {
  syncer::FakeSyncChangeProcessor* processor =
      StartSyncing({CreateSyncData(111)});

  EXPECT_TRUE(processor->changes().empty());
}

// Ensures an error is returned if multiple theme specifics are provided during
// merge.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataFailsWithTooManyItems) {
  syncer::SyncDataList data = {CreateSyncData(111, "hash1"),
                               CreateSyncData(222, "hash2")};

  std::optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, data,
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeTooManySpecifics, error->type());
}

// Ensures an error is returned if the sync data is missing iOS theme specifics.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataFailsWithMissingSpecifics) {
  sync_pb::EntitySpecifics wrong_specifics;
  wrong_specifics.mutable_preference();  // Incorrect specifics type
  syncer::SyncData data = syncer::SyncData::CreateRemoteData(
      wrong_specifics,
      syncer::ClientTagHash::FromUnhashed(syncer::THEMES_IOS, "hash"));

  std::optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, {data},
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeMissingSpecifics, error->type());
}

// Checks that remote data is not applied if the current theme is marked as
// unsyncable.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataDoesNotApplyIfNotSyncable) {
  EXPECT_CALL(delegate_, IsCurrentThemeSyncable()).WillOnce(Return(false));
  EXPECT_CALL(delegate_, ApplyTheme(_)).Times(0);

  syncer::FakeSyncChangeProcessor* processor =
      StartSyncing({CreateSyncData(123)});

  EXPECT_TRUE(processor->changes().empty());
}

// Checks that the theme is not re-applied if the remote theme matches the local
// theme.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataDoesNotApplyIfSameTheme) {
  ON_CALL(delegate_, GetCurrentTheme())
      .WillByDefault(Return(CreateTestTheme(123)));
  EXPECT_CALL(delegate_, ApplyTheme(_)).Times(0);

  syncer::FakeSyncChangeProcessor* processor =
      StartSyncing({CreateSyncData(123)});

  EXPECT_TRUE(processor->changes().empty());
}

// Verifies that a remote theme is applied locally if it differs from the
// current theme.
TEST_F(ThemeSyncableServiceIOSTest, MergeDataAppliesLocallyIfDifferentTheme) {
  ON_CALL(delegate_, GetCurrentTheme())
      .WillByDefault(Return(CreateTestTheme(111)));
  EXPECT_CALL(delegate_, ApplyTheme(EqualsProto(CreateTestTheme(222))))
      .Times(1);

  syncer::FakeSyncChangeProcessor* processor =
      StartSyncing({CreateSyncData(222)});

  EXPECT_TRUE(processor->changes().empty());
}

#pragma mark - ProcessSyncChanges Tests

// Ensures changes are rejected if processed before the sync service has
// started.
TEST_F(ThemeSyncableServiceIOSTest, ProcessChangesFailsIfSyncNotStarted) {
  std::optional<syncer::ModelError> error =
      service_->ProcessSyncChanges(FROM_HERE, {});

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeSyncableServiceNotStarted,
            error->type());
}

// Ensures an error is returned if multiple sync changes are processed at once.
TEST_F(ThemeSyncableServiceIOSTest, ProcessChangesFailsWithMultipleChanges) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  syncer::SyncChangeList changes = {
      CreateSyncChange(syncer::SyncChange::ACTION_UPDATE, 111),
      CreateSyncChange(syncer::SyncChange::ACTION_UPDATE, 222)};

  std::optional<syncer::ModelError> error =
      service_->ProcessSyncChanges(FROM_HERE, changes);

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeTooManyChanges, error->type());

  // Ensure nothing was committed during the failure.
  EXPECT_TRUE(processor->changes().empty());
}

// Verifies that processing a delete action results in an error.
TEST_F(ThemeSyncableServiceIOSTest, ProcessChangesFailsOnDeleteAction) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  syncer::SyncChangeList changes = {
      CreateSyncChange(syncer::SyncChange::ACTION_DELETE, 123)};

  std::optional<syncer::ModelError> error =
      service_->ProcessSyncChanges(FROM_HERE, changes);

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeInvalidChangeType, error->type());

  // Ensure nothing was committed during the failure.
  EXPECT_TRUE(processor->changes().empty());
}

// Ensures an error is returned if an updated sync change lacks iOS theme
// specifics.
TEST_F(ThemeSyncableServiceIOSTest, ProcessChangesFailsWithMissingSpecifics) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  sync_pb::EntitySpecifics wrong_specifics;
  wrong_specifics.mutable_preference();
  syncer::SyncData data = syncer::SyncData::CreateRemoteData(
      wrong_specifics,
      syncer::ClientTagHash::FromUnhashed(syncer::THEMES_IOS, "hash"));

  std::optional<syncer::ModelError> error = service_->ProcessSyncChanges(
      FROM_HERE,
      {syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE, data)});

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(syncer::ModelError::Type::kThemeMissingSpecifics, error->type());

  // Ensure nothing was committed during the failure.
  EXPECT_TRUE(processor->changes().empty());
}

// Checks that remote updates are ignored if the local theme cannot be synced.
TEST_F(ThemeSyncableServiceIOSTest, ProcessChangesIgnoredIfNotSyncable) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  EXPECT_CALL(delegate_, IsCurrentThemeSyncable()).WillOnce(Return(false));
  EXPECT_CALL(delegate_, ApplyTheme(_)).Times(0);

  std::optional<syncer::ModelError> error = service_->ProcessSyncChanges(
      FROM_HERE, {CreateSyncChange(syncer::SyncChange::ACTION_UPDATE, 123)});

  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(processor->changes().empty());
}

// Checks that remote updates are ignored if the incoming theme is identical to
// the local one.
TEST_F(ThemeSyncableServiceIOSTest, ProcessChangesIgnoredIfSameTheme) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  ON_CALL(delegate_, GetCurrentTheme())
      .WillByDefault(Return(CreateTestTheme(123)));
  EXPECT_CALL(delegate_, ApplyTheme(_)).Times(0);

  std::optional<syncer::ModelError> error = service_->ProcessSyncChanges(
      FROM_HERE, {CreateSyncChange(syncer::SyncChange::ACTION_UPDATE, 123)});

  EXPECT_FALSE(error.has_value());
  EXPECT_TRUE(processor->changes().empty());
}

// Verifies remote updates apply locally without echoing the change back to the
// server.
TEST_F(ThemeSyncableServiceIOSTest,
       ProcessChangesAppliesThemeAndPreventsEchoLoop) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  ON_CALL(delegate_, GetCurrentTheme())
      .WillByDefault(Return(CreateTestTheme(111)));

  // Applying the theme locally simulates the UI firing OnThemeChanged.
  EXPECT_CALL(delegate_, ApplyTheme(EqualsProto(CreateTestTheme(222))))
      .WillOnce([this](const sync_pb::ThemeIosSpecifics& theme) {
        service_->OnThemeChanged();
      });

  std::optional<syncer::ModelError> error = service_->ProcessSyncChanges(
      FROM_HERE, {CreateSyncChange(syncer::SyncChange::ACTION_UPDATE, 222)});

  EXPECT_FALSE(error.has_value());
  // The change from the server should not have bounced back into the outgoing
  // queue.
  EXPECT_TRUE(processor->changes().empty());
}

#pragma mark - OnThemeChanged (Local UI Changes) Tests

// Ensures local theme changes are safely ignored before sync starts.
TEST_F(ThemeSyncableServiceIOSTest, OnThemeChangedIgnoredBeforeSyncStarted) {
  service_->OnThemeChanged();
}

// Ensures local theme changes are ignored if the current theme is not syncable.
TEST_F(ThemeSyncableServiceIOSTest, OnThemeChangedIgnoredIfNotSyncable) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  EXPECT_CALL(delegate_, IsCurrentThemeSyncable()).WillOnce(Return(false));

  service_->OnThemeChanged();

  EXPECT_TRUE(processor->changes().empty());
}

// Verifies an ACTION_UPDATE is sent to the server if a local change occurs.
TEST_F(ThemeSyncableServiceIOSTest, OnThemeChangedSendsUpdate) {
  syncer::FakeSyncChangeProcessor* processor = StartSyncing();
  ON_CALL(delegate_, GetCurrentTheme())
      .WillByDefault(Return(CreateTestTheme(555)));

  service_->OnThemeChanged();

  ASSERT_EQ(1u, processor->changes().size());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            processor->changes()[0].change_type());
  EXPECT_EQ(555u, processor->changes()[0]
                      .sync_data()
                      .GetSpecifics()
                      .theme_ios()
                      .user_color_theme()
                      .color());
}

#pragma mark - Stop / Clear Data Tests

// Verifies the cached theme is restored when syncing is explicitly stopped.
TEST_F(ThemeSyncableServiceIOSTest, StopSyncingRestoresCachedTheme) {
  StartSyncing();
  EXPECT_CALL(delegate_, RestoreCachedTheme()).Times(1);

  service_->StopSyncing(syncer::THEMES_IOS);
}

// Checks that the IsSyncing state accurately reflects the service's current
// running state.
TEST_F(ThemeSyncableServiceIOSTest, IsSyncingUpdatesCorrectly) {
  EXPECT_FALSE(service_->IsSyncing());

  StartSyncing();

  EXPECT_TRUE(service_->IsSyncing());

  service_->StopSyncing(syncer::THEMES_IOS);

  EXPECT_FALSE(service_->IsSyncing());
}

// Verifies the cached theme is restored when sync is stopped and data is
// cleared.
TEST_F(ThemeSyncableServiceIOSTest,
       StayStoppedAndMaybeClearDataRestoresCachedTheme) {
  StartSyncing();
  EXPECT_CALL(delegate_, RestoreCachedTheme()).Times(1);

  service_->StayStoppedAndMaybeClearData(syncer::THEMES_IOS);
}

// Ensures the cached theme is not restored during a normal browser shutdown.
TEST_F(ThemeSyncableServiceIOSTest,
       OnBrowserShutdownDoesNotRestoreCachedTheme) {
  StartSyncing();
  EXPECT_CALL(delegate_, RestoreCachedTheme()).Times(0);

  service_->OnBrowserShutdown(syncer::THEMES_IOS);
}

}  // namespace
