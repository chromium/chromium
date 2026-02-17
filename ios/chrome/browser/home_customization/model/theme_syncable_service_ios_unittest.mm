// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/sync/base/client_tag_hash.h"
#import "components/sync/base/data_type.h"
#import "components/sync/model/model_error.h"
#import "components/sync/model/sync_change_processor.h"
#import "components/sync/model/sync_data.h"
#import "components/sync/protocol/entity_data.h"
#import "components/sync/protocol/entity_specifics.pb.h"
#import "components/sync/protocol/theme_ios_specifics.pb.h"
#import "components/sync/test/fake_sync_change_processor.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ThemeSyncableServiceIOSTest : public PlatformTest {
 public:
  ThemeSyncableServiceIOSTest() {
    service_ = std::make_unique<ThemeSyncableServiceIOS>();
  }

 protected:
  // Helper to create valid remote sync data.
  syncer::SyncData CreateRemoteData(const std::string& hash) {
    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_theme_ios();
    return syncer::SyncData::CreateRemoteData(
        entity_specifics,
        syncer::ClientTagHash::FromUnhashed(syncer::THEMES_IOS, hash));
  }
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ThemeSyncableServiceIOS> service_;
};

// Tests that the service returns the specific Client Tag expected by the
// server.
TEST_F(ThemeSyncableServiceIOSTest, ShouldReturnCorrectClientTag) {
  EXPECT_EQ("current_theme_ios", service_->GetClientTag(syncer::EntityData()));
}

// Tests that WaitUntilReadyToSync runs the callback immediately.
TEST_F(ThemeSyncableServiceIOSTest, ShouldRunWaitUntilReadyToSync) {
  base::RunLoop run_loop;
  service_->WaitUntilReadyToSync(run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that starting sync with valid data (empty/initial) returns no error.
TEST_F(ThemeSyncableServiceIOSTest, ShouldStartSyncingWithNoData) {
  std::optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  EXPECT_FALSE(error.has_value());
}

// Tests that starting sync with exactly one item returns no error.
TEST_F(ThemeSyncableServiceIOSTest, ShouldStartSyncingWithOneItem) {
  syncer::SyncDataList data_list;
  data_list.push_back(CreateRemoteData("1"));

  std::optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  EXPECT_FALSE(error.has_value());
}

// Tests that the service enforces the singleton invariant (>1 item is an
// error).
TEST_F(ThemeSyncableServiceIOSTest, ShouldReportErrorIfTooManyItems) {
  syncer::SyncDataList data_list;
  data_list.push_back(CreateRemoteData("1"));
  data_list.push_back(CreateRemoteData("2"));

  std::optional<syncer::ModelError> error = service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, data_list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->type(), syncer::ModelError::Type::kThemeTooManySpecifics);
}

// Tests that StopSyncing works without crashing.
TEST_F(ThemeSyncableServiceIOSTest, ShouldStopSyncing) {
  // Start first to set up state.
  service_->MergeDataAndStartSyncing(
      syncer::THEMES_IOS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  service_->StopSyncing(syncer::THEMES_IOS);
}
