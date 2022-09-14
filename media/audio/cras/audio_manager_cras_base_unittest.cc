// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras_base.h"

#include "base/test/task_environment.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/mock_aecdump_recording_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace media {

namespace {

class MockAecdumpRecordingSource : public AecdumpRecordingSource {
 public:
  MOCK_METHOD1(StartAecdump, void(base::File));
  MOCK_METHOD0(StopAecdump, void());
};

class MockAudioManagerCrasBase : public AudioManagerCrasBase {
 public:
  MockAudioManagerCrasBase()
      : AudioManagerCrasBase(std::make_unique<TestAudioThread>(),
                             &fake_audio_log_factory_) {}

  bool HasAudioOutputDevices() { return true; }
  bool HasAudioInputDevices() { return true; }
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) {
    return AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                           ChannelLayoutConfig::Stereo(), 44100, 1000);
  }
  bool IsDefault(const std::string& device_id, bool is_input) override {
    return true;
  }
  enum CRAS_CLIENT_TYPE GetClientType() { return CRAS_CLIENT_TYPE_LACROS; }

  // We need to override this function in order to skip checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioInputStream, and we don't use
  // MakeAudioInputStream to create the stream in the tests.
  void ReleaseInputStream(AudioInputStream* stream) override {
    DCHECK(stream);
    delete stream;
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class AudioManagerCrasBaseTest : public testing::Test {
 protected:
  AudioManagerCrasBaseTest() {
    mock_manager_.reset(new StrictMock<MockAudioManagerCrasBase>());
    base::RunLoop().RunUntilIdle();
  }
  ~AudioManagerCrasBaseTest() override { mock_manager_->Shutdown(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockAudioManagerCrasBase>> mock_manager_ = NULL;
};

TEST_F(AudioManagerCrasBaseTest, SetAecDumpRecordingManager) {
  MockAecdumpRecordingManager* mock_aecdump_recording_manager =
      new MockAecdumpRecordingManager(mock_manager_->GetTaskRunner());
  mock_manager_->SetAecDumpRecordingManager(
      mock_aecdump_recording_manager->AsWeakPtr());

  MockAecdumpRecordingSource* source = new MockAecdumpRecordingSource();

  EXPECT_CALL(*mock_aecdump_recording_manager, RegisterAecdumpSource(_));
  EXPECT_CALL(*mock_aecdump_recording_manager, DeregisterAecdumpSource(_));

  mock_manager_->RegisterSystemAecDumpSource(source);
  mock_manager_->DeregisterSystemAecDumpSource(source);

  delete mock_aecdump_recording_manager;
}

}  // namespace
}  // namespace media
