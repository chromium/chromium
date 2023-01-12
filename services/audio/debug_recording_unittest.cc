// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/debug_recording.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/audio/audio_debug_recording_test.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/debug_recording_session.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Exactly;
using testing::Sequence;

namespace audio {

namespace {

const base::FilePath::CharType kBaseFileName[] =
    FILE_PATH_LITERAL("base_file_name");

// Empty function bound and passed to DebugRecording::CreateWavFile and
// DebugRecording::CreateAecdumpFile.

void FileCreated(base::File file) {}

}  // namespace

class MockFileProvider : public mojom::DebugRecordingFileProvider {
 public:
  MockFileProvider(
      mojo::PendingReceiver<mojom::DebugRecordingFileProvider> receiver,
      const base::FilePath& file_name_base)
      : receiver_(this, std::move(receiver)) {}

  MockFileProvider(const MockFileProvider&) = delete;
  MockFileProvider& operator=(const MockFileProvider&) = delete;

  MOCK_METHOD2(DoCreateWavFile,
               void(media::AudioDebugRecordingStreamType stream_type,
                    uint32_t id));
  MOCK_METHOD1(DoCreateAecdumpFile, void(uint32_t id));

  void CreateWavFile(media::AudioDebugRecordingStreamType stream_type,
                     uint32_t id,
                     CreateWavFileCallback reply_callback) override {
    DoCreateWavFile(stream_type, id);
    std::move(reply_callback).Run(base::File());
  }

  void CreateAecdumpFile(uint32_t id,
                         CreateAecdumpFileCallback reply_callback) override {
    DoCreateAecdumpFile(id);
    std::move(reply_callback).Run(base::File());
  }

 private:
  mojo::Receiver<mojom::DebugRecordingFileProvider> receiver_;
};

class MockAecdumpRecordingManager : public media::AecdumpRecordingManager {
 public:
  explicit MockAecdumpRecordingManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : AecdumpRecordingManager(task_runner) {}

  MOCK_METHOD1(EnableDebugRecording, void(CreateFileCallback));
  MOCK_METHOD0(DisableDebugRecording, void());
};

class DebugRecordingTest : public media::AudioDebugRecordingTest {
 public:
  DebugRecordingTest() = default;

  DebugRecordingTest(const DebugRecordingTest&) = delete;
  DebugRecordingTest& operator=(const DebugRecordingTest&) = delete;

  ~DebugRecordingTest() override = default;

  void SetUp() override {
    CreateAudioManager();
    InitializeAudioDebugRecordingManager();
    mock_aecdump_recording_manager_ =
        std::make_unique<MockAecdumpRecordingManager>(
            mock_audio_manager_->GetTaskRunner());
  }

  void TearDown() override { ShutdownAudioManager(); }

 protected:
  void CreateDebugRecording() {
    if (remote_debug_recording_)
      remote_debug_recording_.reset();
    debug_recording_ = std::make_unique<DebugRecording>(
        remote_debug_recording_.BindNewPipeAndPassReceiver(),
        static_cast<media::AudioManager*>(mock_audio_manager_.get()),
        mock_aecdump_recording_manager_.get());
  }

  void EnableDebugRecording() {
    mojo::PendingRemote<mojom::DebugRecordingFileProvider> remote_file_provider;
    DebugRecordingSession::DebugRecordingFileProvider file_provider(
        remote_file_provider.InitWithNewPipeAndPassReceiver(),
        base::FilePath(kBaseFileName));
    remote_debug_recording_->Enable(std::move(remote_file_provider));
  }

  void DestroyDebugRecording() {
    remote_debug_recording_.reset();
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<MockAecdumpRecordingManager> mock_aecdump_recording_manager_;
  std::unique_ptr<DebugRecording> debug_recording_;
  mojo::Remote<mojom::DebugRecording> remote_debug_recording_;
};

TEST_F(DebugRecordingTest, EnableResetEnablesDisablesDebugRecording) {
  Sequence s1;
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(_))
      .InSequence(s1);
  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording())
      .InSequence(s1);
  Sequence s2;
  EXPECT_CALL(*mock_aecdump_recording_manager_, EnableDebugRecording(_))
      .InSequence(s2);
  EXPECT_CALL(*mock_aecdump_recording_manager_, DisableDebugRecording())
      .InSequence(s2);

  CreateDebugRecording();
  EnableDebugRecording();
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, ResetWithoutEnableDoesNotDisableDebugRecording) {
  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording()).Times(0);
  EXPECT_CALL(*mock_aecdump_recording_manager_, DisableDebugRecording())
      .Times(0);

  CreateDebugRecording();
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, CreateFileCallsFileProviderCreateFile) {
  Sequence s1;
  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(_))
      .InSequence(s1);
  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording())
      .InSequence(s1);
  Sequence s2;
  EXPECT_CALL(*mock_aecdump_recording_manager_, EnableDebugRecording(_))
      .InSequence(s2);
  EXPECT_CALL(*mock_aecdump_recording_manager_, DisableDebugRecording())
      .InSequence(s2);

  CreateDebugRecording();

  mojo::PendingRemote<mojom::DebugRecordingFileProvider> remote_file_provider;
  MockFileProvider mock_file_provider(
      remote_file_provider.InitWithNewPipeAndPassReceiver(),
      base::FilePath(kBaseFileName));
  remote_debug_recording_->Enable(std::move(remote_file_provider));
  task_environment_.RunUntilIdle();

  const int id = 1;
  EXPECT_CALL(
      mock_file_provider,
      DoCreateWavFile(media::AudioDebugRecordingStreamType::kInput, id));
  EXPECT_CALL(mock_file_provider, DoCreateAecdumpFile(id));

  debug_recording_->CreateWavFile(media::AudioDebugRecordingStreamType::kInput,
                                  id, base::BindOnce(&FileCreated));
  debug_recording_->CreateAecdumpFile(id, base::BindOnce(&FileCreated));
  task_environment_.RunUntilIdle();

  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, SequencialCreate) {
  CreateDebugRecording();
  DestroyDebugRecording();
  CreateDebugRecording();
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, ConcurrentCreate) {
  CreateDebugRecording();
  CreateDebugRecording();
  DestroyDebugRecording();
}

}  // namespace audio
