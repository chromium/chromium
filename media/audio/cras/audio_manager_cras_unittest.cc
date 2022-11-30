// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include "base/test/task_environment.h"
#include "media/audio/cras/cras_util.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"

using testing::StrictMock;

namespace media {

namespace {

class MockCrasUtil : public CrasUtil {
 public:
  MOCK_METHOD(std::vector<CrasDevice>,
              CrasGetAudioDevices,
              (DeviceType type),
              (override));
  MOCK_METHOD(int, CrasGetAecSupported, (), (override));
  MOCK_METHOD(int, CrasGetAecGroupId, (), (override));
  MOCK_METHOD(int, CrasGetDefaultOutputBufferSize, (), (override));
};

class MockAudioManagerCras : public AudioManagerCras {
 public:
  MockAudioManagerCras()
      : AudioManagerCras(std::make_unique<TestAudioThread>(),
                         &fake_audio_log_factory_) {}
  ~MockAudioManagerCras() = default;
  void SetCrasUtil(std::unique_ptr<CrasUtil> util) {
    cras_util_ = std::move(util);
  }
  using AudioManagerCras::GetPreferredOutputStreamParameters;

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class AudioManagerCrasTest : public testing::Test {
 protected:
  AudioManagerCrasTest() {
    mock_manager_.reset(new StrictMock<MockAudioManagerCras>());
    base::RunLoop().RunUntilIdle();
  }
  ~AudioManagerCrasTest() override { mock_manager_->Shutdown(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockAudioManagerCras>> mock_manager_ = NULL;
};

TEST_F(AudioManagerCrasTest, HasAudioInputDevices) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  CrasDevice dev;
  dev.type = DeviceType::kInput;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  auto ret = mock_manager_->HasAudioInputDevices();
  EXPECT_EQ(ret, true);
}

TEST_F(AudioManagerCrasTest, CheckDefaultNoDevice) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  AudioDeviceNames device_names;
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillOnce(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  mock_manager_->GetAudioInputDeviceNames(&device_names);
  EXPECT_EQ(device_names.empty(), true);
  mock_manager_->GetAudioOutputDeviceNames(&device_names);
  EXPECT_EQ(device_names.empty(), true);
}

TEST_F(AudioManagerCrasTest, CheckDefaultDevice) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  AudioDeviceNames device_names;
  CrasDevice dev;
  dev.type = DeviceType::kInput;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  mock_manager_->GetAudioInputDeviceNames(&device_names);
  EXPECT_EQ(device_names.size(), 2u);
}

TEST_F(AudioManagerCrasTest, MaxChannel) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  CrasDevice dev;
  dev.type = DeviceType::kOutput;
  dev.id = 123;
  dev.max_supported_channels = 6;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetDefaultOutputBufferSize());
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillRepeatedly(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  auto params = mock_manager_->GetPreferredOutputStreamParameters(
      "123", AudioParameters());
  EXPECT_EQ(params.channels(), 6);
}

}  // namespace

}  // namespace media
