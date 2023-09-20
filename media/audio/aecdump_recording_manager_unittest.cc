// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/aecdump_recording_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::NiceMock;
using testing::Sequence;

namespace media {
namespace {

class MockAecdumpRecordingSource : public AecdumpRecordingSource {
 public:
  MOCK_METHOD1(StartAecdump, void(base::File));
  MOCK_METHOD0(StopAecdump, void());
};

class AecdumpRecordingManagerTest : public ::testing::Test {
 public:
  AecdumpRecordingManagerTest()
      : manager_(std::make_unique<AecdumpRecordingManager>(
            task_environment_.GetMainThreadTaskRunner())),
        create_file_callback_(base::BindRepeating(
            &AecdumpRecordingManagerTest::AsyncCreateFileCallback,
            base::Unretained(this))) {}

  void SetUp() override {
    temp_dir_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

 protected:
  // Simulates async file creation by posting the reply to the main thread.
  void AsyncCreateFileCallback(
      uint32_t /*id*/,
      base::OnceCallback<void(base::File)> reply_callback) {
    base::FilePath temp_file_path;
    base::File file =
        base::CreateAndOpenTemporaryFileInDir(temp_dir_path_, &temp_file_path);
    ASSERT_TRUE(file.IsValid());
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply_callback), std::move(file)));
  }

  base::FilePath temp_dir_path_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AecdumpRecordingManager> manager_;
  AecdumpRecordingManager::CreateFileCallback create_file_callback_;
};

TEST_F(AecdumpRecordingManagerTest, EnableDisableDoesNotCrash) {
  manager_->EnableDebugRecording(create_file_callback_);
  manager_->DisableDebugRecording();
}

TEST_F(AecdumpRecordingManagerTest,
       EnableDisableWithOneSourceStartsStopsSource) {
  MockAecdumpRecordingSource mock_source;
  InSequence s;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(1);
  EXPECT_CALL(mock_source, StopAecdump()).Times(1);

  manager_->RegisterAecdumpSource(&mock_source);

  manager_->EnableDebugRecording(create_file_callback_);
  base::RunLoop().RunUntilIdle();

  manager_->DisableDebugRecording();

  manager_->DeregisterAecdumpSource(&mock_source);
}

TEST_F(AecdumpRecordingManagerTest,
       EnableDisableWithTwoSourcesStartsStopsSources) {
  MockAecdumpRecordingSource mock_source_a;
  MockAecdumpRecordingSource mock_source_b;
  Sequence s1;
  Sequence s2;
  EXPECT_CALL(mock_source_a, StartAecdump(_)).Times(1).InSequence(s1);
  EXPECT_CALL(mock_source_a, StopAecdump()).Times(1).InSequence(s1);
  EXPECT_CALL(mock_source_b, StartAecdump(_)).Times(1).InSequence(s2);
  EXPECT_CALL(mock_source_b, StopAecdump()).Times(1).InSequence(s2);

  manager_->RegisterAecdumpSource(&mock_source_a);
  manager_->RegisterAecdumpSource(&mock_source_b);

  manager_->EnableDebugRecording(create_file_callback_);
  base::RunLoop().RunUntilIdle();

  manager_->DisableDebugRecording();

  manager_->DeregisterAecdumpSource(&mock_source_a);
  manager_->DeregisterAecdumpSource(&mock_source_b);
}

TEST_F(AecdumpRecordingManagerTest,
       RegisterDeregisterSourceBeforeEnableDisableDoesNotStartStopSource) {
  MockAecdumpRecordingSource mock_source;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(0);
  EXPECT_CALL(mock_source, StopAecdump()).Times(0);

  manager_->RegisterAecdumpSource(&mock_source);
  manager_->DeregisterAecdumpSource(&mock_source);

  // Enabling debug recordings should not trigger any calls to the recording
  // source.
  manager_->EnableDebugRecording(create_file_callback_);
  base::RunLoop().RunUntilIdle();
  manager_->DisableDebugRecording();
}

TEST_F(AecdumpRecordingManagerTest,
       RegisterDeregisterSourceAfterEnableDisableDoesNotStartStopSource) {
  MockAecdumpRecordingSource mock_source;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(0);
  EXPECT_CALL(mock_source, StopAecdump()).Times(0);

  // Enabling debug recordings should not trigger any calls to the recording
  // source.
  manager_->EnableDebugRecording(create_file_callback_);
  manager_->DisableDebugRecording();

  manager_->RegisterAecdumpSource(&mock_source);
  base::RunLoop().RunUntilIdle();
  manager_->DeregisterAecdumpSource(&mock_source);
}

TEST_F(AecdumpRecordingManagerTest,
       RegisterDeregisterSourceDuringRecordingStartsStopsSource) {
  InSequence s;
  MockAecdumpRecordingSource mock_source;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(1);
  EXPECT_CALL(mock_source, StopAecdump()).Times(1);

  manager_->EnableDebugRecording(create_file_callback_);

  manager_->RegisterAecdumpSource(&mock_source);
  base::RunLoop().RunUntilIdle();

  manager_->DeregisterAecdumpSource(&mock_source);
  ::testing::Mock::VerifyAndClearExpectations(&mock_source);

  manager_->DisableDebugRecording();
}

TEST_F(AecdumpRecordingManagerTest,
       DeregisterSourceBeforeFileCreationCallbackDoesNotStartSource) {
  NiceMock<MockAecdumpRecordingSource> mock_source;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(0);

  manager_->RegisterAecdumpSource(&mock_source);

  // Enabling debug recordings schedules a StartAecdump() call to the recording
  // source.
  manager_->EnableDebugRecording(create_file_callback_);

  // Deregister the source before StartAecdump() can be called.
  manager_->DeregisterAecdumpSource(&mock_source);

  // Run the file creation callback. Aecdumps should not be started.
  base::RunLoop().RunUntilIdle();

  manager_->DisableDebugRecording();
}

TEST_F(AecdumpRecordingManagerTest,
       StopRecordingBeforeFileCreationCallbackDoesNotStartSource) {
  NiceMock<MockAecdumpRecordingSource> mock_source;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(0);

  manager_->RegisterAecdumpSource(&mock_source);

  // Enabling debug recordings schedules a StartAecdump() call to the recording
  // source.
  manager_->EnableDebugRecording(create_file_callback_);

  // Stop aecdump recording before StartAecdump() can be called.
  manager_->DisableDebugRecording();

  // Run the file creation callback. Aecdumps should not be started.
  base::RunLoop().RunUntilIdle();

  // Clean up.
  manager_->DeregisterAecdumpSource(&mock_source);
}

TEST_F(AecdumpRecordingManagerTest,
       RestartRecordingBeforeFileCreationCallbackDoesNotStartSourceTwice) {
  NiceMock<MockAecdumpRecordingSource> mock_source;
  EXPECT_CALL(mock_source, StartAecdump(_)).Times(1);

  manager_->RegisterAecdumpSource(&mock_source);

  // Enabling debug recordings schedules a StartAecdump() call to the recording
  // source.
  manager_->EnableDebugRecording(create_file_callback_);

  // Restart aecdump recording before StartAecdump() can be called.
  manager_->DisableDebugRecording();
  manager_->EnableDebugRecording(create_file_callback_);

  // Run the file creation callback. The first file created should be discarded,
  // we expect only one StartAecdump() call.
  base::RunLoop().RunUntilIdle();

  // Clean up.
  manager_->DeregisterAecdumpSource(&mock_source);
  manager_->DisableDebugRecording();
}

TEST_F(AecdumpRecordingManagerTest,
       DestroyManagerBeforeFileCreationCallbackDoesNotCrash) {
  NiceMock<MockAecdumpRecordingSource> mock_source;
  manager_->RegisterAecdumpSource(&mock_source);

  // Enabling debug recordings schedules a StartAecdump() call to the recording
  // source.
  manager_->EnableDebugRecording(create_file_callback_);

  // Destroy the AecdumpRecordingManager before StartAecdump() can be called.
  manager_->DeregisterAecdumpSource(&mock_source);
  manager_->DisableDebugRecording();
  manager_.reset();

  // Run the file creation callback. Aecdumps should not be started.
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace media
