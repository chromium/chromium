// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/debug_recording_session.h"

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/audio/audio_debug_recording_test.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

namespace {

#if defined(OS_WIN)
#define NumberToStringType base::NumberToString16
#else
#define NumberToStringType base::NumberToString
#endif

const base::FilePath::CharType kBaseFileName[] =
    FILE_PATH_LITERAL("debug_recording");
const base::FilePath::CharType kWavExtension[] = FILE_PATH_LITERAL("wav");
const base::FilePath::CharType kInput[] = FILE_PATH_LITERAL("input");
const base::FilePath::CharType kOutput[] = FILE_PATH_LITERAL("output");

}  // namespace

class DebugRecordingFileProviderTest : public testing::Test {
 public:
  DebugRecordingFileProviderTest() = default;
  ~DebugRecordingFileProviderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(kBaseFileName);
    file_provider_ =
        std::make_unique<DebugRecordingSession::DebugRecordingFileProvider>(
            remote_file_provider_.BindNewPipeAndPassReceiver(), file_path_);
  }

  void TearDown() override { file_provider_.reset(); }

  base::FilePath GetFileName(const base::FilePath::StringType& stream_type,
                             uint32_t id) {
    return file_path_.AddExtension(stream_type)
        .AddExtension(NumberToStringType(id))
        .AddExtension(kWavExtension);
  }

  MOCK_METHOD1(OnFileCreated, void(bool));
  void FileCreated(base::File file) { OnFileCreated(file.IsValid()); }

 protected:
  mojo::Remote<mojom::DebugRecordingFileProvider> remote_file_provider_;
  base::test::TaskEnvironment task_environment_;

 private:
  std::unique_ptr<DebugRecordingSession::DebugRecordingFileProvider>
      file_provider_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(DebugRecordingFileProviderTest);
};

class DebugRecordingSessionTest : public media::AudioDebugRecordingTest {
 public:
  DebugRecordingSessionTest() = default;
  ~DebugRecordingSessionTest() override = default;

  void SetUp() override {
    CreateAudioManager();
    InitializeAudioDebugRecordingManager();

    service_ = CreateEmbeddedService(
        static_cast<media::AudioManager*>(mock_audio_manager_.get()),
        connector_factory_.RegisterInstance(audio::mojom::kServiceName));

    task_environment_.RunUntilIdle();
  }

  void TearDown() override { ShutdownAudioManager(); }

 protected:
  std::unique_ptr<DebugRecordingSession> CreateDebugRecordingSession() {
    std::unique_ptr<DebugRecordingSession> session(
        std::make_unique<DebugRecordingSession>(
            base::FilePath(kBaseFileName),
            connector_factory_.CreateConnector()));
    task_environment_.RunUntilIdle();
    return session;
  }

  void DestroyDebugRecordingSession(
      std::unique_ptr<DebugRecordingSession> debug_recording_session) {
    debug_recording_session.reset();
    task_environment_.RunUntilIdle();
  }

 private:
  service_manager::TestConnectorFactory connector_factory_;
  std::unique_ptr<Service> service_;

  DISALLOW_COPY_AND_ASSIGN(DebugRecordingSessionTest);
};

TEST_F(DebugRecordingFileProviderTest, CreateFileForInputStream) {
  const uint32_t id = 1;
  EXPECT_CALL(*this, OnFileCreated(true));
  remote_file_provider_->CreateWavFile(
      media::AudioDebugRecordingStreamType::kInput, id,
      base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();

  base::FilePath file_name(GetFileName(kInput, id));
  EXPECT_TRUE(base::PathExists(file_name));
  ASSERT_TRUE(base::DeleteFile(file_name, false));
}

TEST_F(DebugRecordingFileProviderTest, CreateFileForOutputStream) {
  const uint32_t id = 1;
  EXPECT_CALL(*this, OnFileCreated(true));
  remote_file_provider_->CreateWavFile(
      media::AudioDebugRecordingStreamType::kOutput, id,
      base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();

  base::FilePath file_name(GetFileName(kOutput, id));
  EXPECT_TRUE(base::PathExists(file_name));
  ASSERT_TRUE(base::DeleteFile(file_name, false));
}

TEST_F(DebugRecordingFileProviderTest, CreateFilesForVariousIds) {
  uint32_t ids[]{std::numeric_limits<uint32_t>::min(),
                 std::numeric_limits<uint32_t>::max()};
  EXPECT_CALL(*this, OnFileCreated(true)).Times(2);
  for (uint32_t id : ids) {
    remote_file_provider_->CreateWavFile(
        media::AudioDebugRecordingStreamType::kOutput, id,
        base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                       base::Unretained(this)));
  }
  task_environment_.RunUntilIdle();

  for (uint32_t id : ids) {
    base::FilePath file_name(GetFileName(kOutput, id));
    EXPECT_TRUE(base::PathExists(file_name));
    EXPECT_TRUE(base::DeleteFile(file_name, false));
  }
}

TEST_F(DebugRecordingFileProviderTest,
       CreateFileWithInvalidStreamTypeDoesNotCreateFile) {
  const uint32_t invalid_stream_type = 7;
  const uint32_t id = 1;
  EXPECT_CALL(*this, OnFileCreated(true)).Times(0);
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DCHECK_DEATH(remote_file_provider_->CreateWavFile(
      static_cast<media::AudioDebugRecordingStreamType>(invalid_stream_type),
      id,
      base::BindOnce(&DebugRecordingFileProviderTest::FileCreated,
                     base::Unretained(this))));
  task_environment_.RunUntilIdle();

  base::FilePath file_name(
      GetFileName(NumberToStringType(invalid_stream_type), id));
  EXPECT_FALSE(base::PathExists(file_name));
}

TEST_F(DebugRecordingSessionTest,
       CreateDestroySessionEnablesDisablesDebugRecording) {
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  std::unique_ptr<DebugRecordingSession> session =
      CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession(std::move(session));
}

TEST_F(DebugRecordingSessionTest,
       CreateTwoSessionsFirstSessionDestroyedOnSecondSessionCreation) {
  testing::InSequence seq;

  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  std::unique_ptr<DebugRecordingSession> first_session =
      CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  std::unique_ptr<DebugRecordingSession> second_session =
      CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording()).Times(0);
  DestroyDebugRecordingSession(std::move(first_session));
  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession(std::move(second_session));
}

}  // namespace audio
