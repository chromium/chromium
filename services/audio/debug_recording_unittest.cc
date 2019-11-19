// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/debug_recording.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_debug_recording_test.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/audio/mock_audio_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/debug_recording_session.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"
#include "services/audio/traced_service_ref.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "testing/gmock/include/gmock/gmock.h"

    using testing::_;
using testing::Exactly;

namespace audio {

namespace {

const base::FilePath::CharType kBaseFileName[] =
    FILE_PATH_LITERAL("base_file_name");

// Empty function bound and passed to DebugRecording::CreateWavFile.
void FileCreated(base::File file) {}

}  // namespace

class MockFileProvider : public mojom::DebugRecordingFileProvider {
 public:
  MockFileProvider(
      mojo::PendingReceiver<mojom::DebugRecordingFileProvider> receiver,
      const base::FilePath& file_name_base)
      : receiver_(this, std::move(receiver)) {}

  MOCK_METHOD2(DoCreateWavFile,
               void(media::AudioDebugRecordingStreamType stream_type,
                    uint32_t id));
  void CreateWavFile(media::AudioDebugRecordingStreamType stream_type,
                     uint32_t id,
                     CreateWavFileCallback reply_callback) override {
    DoCreateWavFile(stream_type, id);
    std::move(reply_callback).Run(base::File());
  }

 private:
  mojo::Receiver<mojom::DebugRecordingFileProvider> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MockFileProvider);
};

class DebugRecordingTest : public media::AudioDebugRecordingTest,
                           public service_manager::ServiceKeepalive::Observer {
 public:
  DebugRecordingTest() : service_keepalive_(nullptr, base::TimeDelta()) {
    service_keepalive_.AddObserver(this);
  }

  ~DebugRecordingTest() override = default;

  void SetUp() override {
    CreateAudioManager();
    InitializeAudioDebugRecordingManager();
  }

  void TearDown() override { ShutdownAudioManager(); }

 protected:
  MOCK_METHOD0(OnNoServiceRefs, void());

  void CreateDebugRecording() {
    if (remote_debug_recording_)
      remote_debug_recording_.reset();
    debug_recording_ = std::make_unique<DebugRecording>(
        remote_debug_recording_.BindNewPipeAndPassReceiver(),
        static_cast<media::AudioManager*>(mock_audio_manager_.get()),
        TracedServiceRef(service_keepalive_.CreateRef(),
                         "audio::DebugRecording Binding"));
    EXPECT_FALSE(service_keepalive_.HasNoRefs());
  }

  void EnableDebugRecording() {
    mojo::PendingRemote<mojom::DebugRecordingFileProvider> remote_file_provider;
    DebugRecordingSession::DebugRecordingFileProvider file_provider(
        remote_file_provider.InitWithNewPipeAndPassReceiver(),
        base::FilePath(kBaseFileName));
    remote_debug_recording_->Enable(std::move(remote_file_provider));
    EXPECT_FALSE(service_keepalive_.HasNoRefs());
  }

  void DestroyDebugRecording() {
    remote_debug_recording_.reset();
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(service_keepalive_.HasNoRefs());
  }

  // service_manager::ServiceKeepalive::Observer:
  void OnIdleTimeout() override { OnNoServiceRefs(); }

  std::unique_ptr<DebugRecording> debug_recording_;
  mojo::Remote<mojom::DebugRecording> remote_debug_recording_;
  service_manager::ServiceKeepalive service_keepalive_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugRecordingTest);
};

TEST_F(DebugRecordingTest, EnableResetEnablesDisablesDebugRecording) {
  EXPECT_CALL(*this, OnNoServiceRefs()).Times(Exactly(1));
  CreateDebugRecording();

  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(_));
  EnableDebugRecording();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, ResetWithoutEnableDoesNotDisableDebugRecording) {
  EXPECT_CALL(*this, OnNoServiceRefs()).Times(Exactly(1));
  CreateDebugRecording();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording()).Times(0);
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, CreateWavFileCallsFileProviderCreateWavFile) {
  EXPECT_CALL(*this, OnNoServiceRefs()).Times(Exactly(1));
  CreateDebugRecording();

  mojo::PendingRemote<mojom::DebugRecordingFileProvider> remote_file_provider;
  MockFileProvider mock_file_provider(
      remote_file_provider.InitWithNewPipeAndPassReceiver(),
      base::FilePath(kBaseFileName));

  EXPECT_CALL(*mock_debug_recording_manager_, EnableDebugRecording(_));
  remote_debug_recording_->Enable(std::move(remote_file_provider));
  task_environment_.RunUntilIdle();

  const int id = 1;
  EXPECT_CALL(
      mock_file_provider,
      DoCreateWavFile(media::AudioDebugRecordingStreamType::kInput, id));
  debug_recording_->CreateWavFile(media::AudioDebugRecordingStreamType::kInput,
                                  id, base::BindOnce(&FileCreated));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*mock_debug_recording_manager_, DisableDebugRecording());
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, SequencialCreate) {
  EXPECT_CALL(*this, OnNoServiceRefs()).Times(Exactly(2));
  CreateDebugRecording();
  DestroyDebugRecording();
  CreateDebugRecording();
  DestroyDebugRecording();
}

TEST_F(DebugRecordingTest, ConcurrentCreate) {
  CreateDebugRecording();
  CreateDebugRecording();
  EXPECT_CALL(*this, OnNoServiceRefs());
  DestroyDebugRecording();
}

}  // namespace audio
