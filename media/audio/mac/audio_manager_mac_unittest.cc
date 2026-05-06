// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_manager_mac.h"

#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;

namespace media {
// This class is used to mock audio devices and test their behavior in
// AudioManagerMac.
class AudioManagerMacUnderTest final : public AudioManagerMac {
 public:
  AudioManagerMacUnderTest(std::unique_ptr<AudioThread> audio_thread,
                           AudioLogFactory* audio_log_factory)
      : AudioManagerMac(std::move(audio_thread), audio_log_factory) {}

  MOCK_METHOD0(GetAllAudioDeviceIDs, std::vector<AudioObjectID>());
  MOCK_METHOD1(GetDeviceTransportType, std::optional<uint32_t>(AudioObjectID));
  MOCK_METHOD1(GetDeviceUniqueID, std::optional<std::string>(AudioObjectID));
  MOCK_METHOD1(GetRelatedNonBluetoothDeviceIDs,
               std::vector<AudioObjectID>(AudioObjectID));
};

class AudioManagerMacTest : public ::testing::Test {
 protected:
  AudioManagerMacTest() : message_loop_(base::MessagePumpType::IO) {
    audio_manager_ = std::make_unique<AudioManagerMacUnderTest>(
        std::make_unique<TestAudioThread>(), &fake_audio_log_factory_);
    base::RunLoop().RunUntilIdle();
  }
  ~AudioManagerMacTest() override { audio_manager_->Shutdown(); }

  base::TestMessageLoop message_loop_;
  FakeAudioLogFactory fake_audio_log_factory_;
  std::unique_ptr<AudioManagerMacUnderTest> audio_manager_;
};

// This creates a test which mocks the following input and output bluetooth
// device. The device configuration is as follows:
// Input: DeviceID: 1, uniqueID: "F3-A2-14-A9-1D-F8:input"
// Output: DeviceID: 2, uniqueID: "F3-A2-14-A9-1D-F8:output"
TEST_F(AudioManagerMacTest, SameGroupIdForBluetoothInputAndOutputDevice) {
  AudioManagerMacUnderTest& audio_manager_mock = *audio_manager_.get();
  EXPECT_CALL(audio_manager_mock, GetAllAudioDeviceIDs())
      .WillRepeatedly(Return(std::vector<AudioObjectID>{1, 2}));
  // DeviceID: 1
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/1))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBluetooth));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/1))
      .WillRepeatedly(Return("F3-A2-14-A9-1D-F8:input"));
  // DeviceID: 2
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/2))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBluetooth));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/2))
      .WillRepeatedly(Return("F3-A2-14-A9-1D-F8:output"));

  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/1),
            std::vector<AudioObjectID>({1, 2}));
  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/2),
            std::vector<AudioObjectID>({1, 2}));
}

// This creates a test which mocks 2 related built-in audio devices.
TEST_F(AudioManagerMacTest, SameGroupIdForNonBluetoothInputAndOutputDevice) {
  AudioManagerMacUnderTest& audio_manager_mock = *audio_manager_.get();
  // DeviceID: 1
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/1))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBuiltIn));
  EXPECT_CALL(audio_manager_mock,
              GetRelatedNonBluetoothDeviceIDs(/*device_id=*/1))
      .WillRepeatedly(Return(std::vector<AudioObjectID>{1, 2}));

  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/1),
            std::vector<AudioObjectID>({1, 2}));
}

// This creates a test which mocks the following unrelated input and output
// bluetooth device. The device configuration is as follows:
// Input: DeviceID: 1, uniqueID: "A3-C2-E3-19-D3-81:input"
// Output: DeviceID: 2, uniqueID: "F3-A2-14-A9-1D-F8:output"
TEST_F(AudioManagerMacTest,
       DifferentGroupIdForDifferentBluetoothInputAndOutputDevice) {
  AudioManagerMacUnderTest& audio_manager_mock = *audio_manager_.get();
  EXPECT_CALL(audio_manager_mock, GetAllAudioDeviceIDs())
      .WillRepeatedly(Return(std::vector<AudioObjectID>{1, 2}));
  // DeviceID: 1
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/1))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBluetooth));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/1))
      .WillRepeatedly(Return("A3-C2-E3-19-D3-81:input"));
  // DeviceID: 2
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/2))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBluetooth));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/2))
      .WillRepeatedly(Return("F3-A2-14-A9-1D-F8:output"));

  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/1),
            std::vector<AudioObjectID>{1});
  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/2),
            std::vector<AudioObjectID>{2});
}

// This creates a test which mocks the following unrelated input and output
// devices. The device configuration is as follows:
// Input: DeviceID: 1, uniqueID: "A3-C2-E3-19-D3-81:input"
// Input: DeviceID: 2, uniqueID: "default_input_device"
// Output: DeviceID: 3, uniqueID: "inbuilt_output_device"
TEST_F(AudioManagerMacTest, DifferentGroupIdForDifferentInputAndOutputDevices) {
  AudioManagerMacUnderTest& audio_manager_mock = *audio_manager_.get();
  EXPECT_CALL(audio_manager_mock, GetAllAudioDeviceIDs())
      .WillRepeatedly(Return(std::vector<AudioObjectID>{1, 2, 3}));
  // DeviceID: 1
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/1))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBluetooth));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/1))
      .WillRepeatedly(Return("A3-C2-E3-19-D3-81:input"));
  // DeviceID: 2
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/2))
      .WillRepeatedly(Return(kAudioDeviceTransportTypeBuiltIn));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/2))
      .WillRepeatedly(Return("default_input_device"));
  EXPECT_CALL(audio_manager_mock,
              GetRelatedNonBluetoothDeviceIDs(/*device_id=*/2))
      .WillRepeatedly(Return(std::vector<AudioObjectID>{2}));
  // DeviceID: 3
  EXPECT_CALL(audio_manager_mock, GetDeviceTransportType(/*device_id=*/3))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(audio_manager_mock, GetDeviceUniqueID(/*device_id=*/3))
      .WillRepeatedly(Return("inbuilt_output_device"));
  EXPECT_CALL(audio_manager_mock,
              GetRelatedNonBluetoothDeviceIDs(/*device_id=*/3))
      .WillRepeatedly(Return(std::vector<AudioObjectID>{3}));

  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/1),
            std::vector<AudioObjectID>{1});
  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/2),
            std::vector<AudioObjectID>{2});
  EXPECT_EQ(audio_manager_->GetRelatedDeviceIDs(/*device_id=*/3),
            std::vector<AudioObjectID>{3});
}
}  // namespace media
