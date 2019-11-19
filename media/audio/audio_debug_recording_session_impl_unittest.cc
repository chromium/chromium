// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_session_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/audio/audio_debug_recording_test.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

namespace {

#if defined(OS_WIN)
#define NumberToStringType base::NumberToString16
#else
#define NumberToStringType base::NumberToString
#endif

const base::FilePath::CharType kBaseFileName[] =
    FILE_PATH_LITERAL("debug_recording");
const base::FilePath::CharType kInput[] = FILE_PATH_LITERAL("input");
const base::FilePath::CharType kOutput[] = FILE_PATH_LITERAL("output");
const int kId = 1;
const base::FilePath::CharType kWavExtension[] = FILE_PATH_LITERAL("wav");

void OnFileCreated(base::File debug_file) {}

// Action function called on
// MockAudioDebugRecordingManager::EnableDebugRecording mocked method to test
// |create_file_callback| behavior.
void CreateInputOutputDebugRecordingFiles(
    const AudioDebugRecordingManager::CreateWavFileCallback&
        create_file_callback) {
  create_file_callback.Run(AudioDebugRecordingStreamType::kInput, kId,
                           base::BindOnce(&OnFileCreated));
  create_file_callback.Run(AudioDebugRecordingStreamType::kOutput, kId,
                           base::BindOnce(&OnFileCreated));
}

}  // namespace

class AudioDebugRecordingSessionImplTest : public AudioDebugRecordingTest {
 public:
  AudioDebugRecordingSessionImplTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base_file_path_ = temp_dir_.GetPath().Append(base::FilePath(kBaseFileName));
  }

 protected:
  void CreateDebugRecordingSession() {
    audio_debug_recording_session_impl_ =
        std::make_unique<media::AudioDebugRecordingSessionImpl>(
            base_file_path_);
  }

  void DestroyDebugRecordingSession() {
    audio_debug_recording_session_impl_.reset();
  }

  base::FilePath GetFileName(const base::FilePath::StringType& stream_type,
                             uint32_t id) {
    return base_file_path_.AddExtension(stream_type)
        .AddExtension(NumberToStringType(id))
        .AddExtension(kWavExtension);
  }

  base::FilePath base_file_path_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AudioDebugRecordingSessionImpl>
      audio_debug_recording_session_impl_;

  DISALLOW_COPY_AND_ASSIGN(AudioDebugRecordingSessionImplTest);
};

TEST_F(AudioDebugRecordingSessionImplTest,
       ConstructorEnablesAndDestructorDisablesDebugRecordingOnAudioManager) {
  ::testing::InSequence seq;

  CreateAudioManager();
  InitializeAudioDebugRecordingManager();
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_));
  CreateDebugRecordingSession();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession();

  ShutdownAudioManager();
}

TEST_F(AudioDebugRecordingSessionImplTest,
       CreateDestroySessionDontCrashWithNoAudioManager) {
  ASSERT_EQ(nullptr, AudioManager::Get());
  CreateDebugRecordingSession();
  DestroyDebugRecordingSession();
}

TEST_F(AudioDebugRecordingSessionImplTest,
       CreateDestroySessionDontCrashWithoutInitializingDebugRecordingManager) {
  CreateAudioManager();
  CreateDebugRecordingSession();
  DestroyDebugRecordingSession();
  ShutdownAudioManager();
}

// Tests the CreateWavFile method from AudioDebugRecordingSessionImpl unnamed
// namespace.
TEST_F(AudioDebugRecordingSessionImplTest, CreateWavFileCreatesExpectedFiles) {
  CreateAudioManager();
  InitializeAudioDebugRecordingManager();
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(testing::_))
      .WillOnce(testing::Invoke(CreateInputOutputDebugRecordingFiles));
  CreateDebugRecordingSession();

  // Wait for files to be created.
  task_environment_.RunUntilIdle();

  // Check that expected files were created.
  base::FilePath input_recording_filename(GetFileName(kInput, kId));
  base::FilePath output_recording_filename(GetFileName(kOutput, kId));
  EXPECT_TRUE(base::PathExists(output_recording_filename));
  EXPECT_TRUE(base::PathExists(input_recording_filename));

  // Clean-up.
  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession();
  ShutdownAudioManager();
  EXPECT_TRUE(base::DeleteFile(output_recording_filename, false));
  EXPECT_TRUE(base::DeleteFile(input_recording_filename, false));
}

}  // namespace media
