// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/audio/mac/catap_audio_input_stream.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/catap_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
// A function pointer to this function is used as an identifier for the tap IO
// process ID.
OSStatus AudioDeviceIoProcIdFunction(AudioDeviceID,
                                     const AudioTimeStamp*,
                                     const AudioBufferList*,
                                     const AudioTimeStamp*,
                                     AudioBufferList*,
                                     const AudioTimeStamp*,
                                     void*) {
  return noErr;
}
constexpr AudioDeviceIOProcID kTapIoProcId = AudioDeviceIoProcIdFunction;

// Arbitrary numbers that identifies the aggregate device and tap.
constexpr AudioObjectID kAggregateDeviceId = 17;
constexpr AudioObjectID kTap = 23;

// Mock for the AudioInputCallback.
class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData,
               void(const AudioBus* bus,
                    base::TimeTicks capture_time,
                    double volume,
                    const AudioGlitchInfo& glitch_info));
  MOCK_METHOD0(OnError, void());
};

// Mock for all CoreAudio API calls.
class API_AVAILABLE(macos(14.2)) MockCatapApi : public CatapApi {
 public:
  MockCatapApi() = default;
  ~MockCatapApi() override = default;

  MOCK_METHOD(OSStatus,
              AudioHardwareCreateAggregateDevice,
              (CFDictionaryRef in_device_properties, AudioDeviceID* out_device),
              (override));
  MOCK_METHOD(OSStatus,
              AudioDeviceCreateIOProcID,
              (AudioDeviceID in_device,
               AudioDeviceIOProc proc,
               void* in_client_data,
               AudioDeviceIOProcID* out_proc_id),
              (override));
  MOCK_METHOD(OSStatus,
              AudioObjectGetPropertyData,
              (AudioObjectID in_object_id,
               const AudioObjectPropertyAddress* in_address,
               UInt32 in_qualifier_data_size,
               const void* in_qualifier_data,
               UInt32* ioDataSize,
               void* outData),
              (override));
  MOCK_METHOD(OSStatus,
              AudioObjectSetPropertyData,
              (AudioObjectID in_object_id,
               const AudioObjectPropertyAddress* in_address,
               UInt32 in_qualifier_data_size,
               const void* in_qualifier_data,
               UInt32 in_data_size,
               const void* in_data),
              (override));
  MOCK_METHOD(OSStatus,
              AudioHardwareCreateProcessTap,
              (CATapDescription * in_description, AudioObjectID* out_tap),
              (override));
  MOCK_METHOD(OSStatus,
              AudioDeviceStart,
              (AudioDeviceID in_device, AudioDeviceIOProcID in_proc_id),
              (override));
  MOCK_METHOD(OSStatus,
              AudioDeviceStop,
              (AudioDeviceID in_device, AudioDeviceIOProcID in_proc_id),
              (override));
  MOCK_METHOD(OSStatus,
              AudioDeviceDestroyIOProcID,
              (AudioDeviceID in_device, AudioDeviceIOProcID in_proc_id),
              (override));
  MOCK_METHOD(OSStatus,
              AudioHardwareDestroyAggregateDevice,
              (AudioDeviceID in_device),
              (override));
  MOCK_METHOD(OSStatus,
              AudioHardwareDestroyProcessTap,
              (AudioObjectID in_tap),
              (override));
};

}  // namespace

class CatapAudioInputStreamTest : public testing::Test {
 public:
  CatapAudioInputStreamTest() = default;
  ~CatapAudioInputStreamTest() override = default;

  AudioInputStream::OpenOutcome CreateAndOpenStream(bool with_permissions) {
    if (@available(macOS 14.2, *)) {
      auto mock_catap_api_object = std::make_unique<MockCatapApi>();
      // Keep a raw pointer to set expectations.
      mock_catap_api_ = mock_catap_api_object.get();

      // Create a default CatapAudioInputStream for testing.
      stream_ = CreateCatapAudioInputStreamForTesting(
          AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                          ChannelLayoutConfig::Stereo(), kLoopbackSampleRate,
                          kCatapLoopbackFramesPerBuffer),
          media::AudioDeviceDescription::kLoopbackInputDeviceId,
          base::DoNothing(), base::DoNothing(),
          media::AudioDeviceDescription::kDefaultDeviceId,
          std::move(mock_catap_api_object));
      EXPECT_TRUE(stream_);

      // Set up expectations for a successful open.
      EXPECT_CALL(mock_catap_api(), AudioHardwareCreateProcessTap)
          .Times(1)
          .WillOnce(
              [](CATapDescription* in_description, AudioObjectID* out_tap) {
                *out_tap = kTap;
                return noErr;
              });
      EXPECT_CALL(mock_catap_api(), AudioHardwareCreateAggregateDevice)
          .Times(1)
          .WillOnce([](CFDictionaryRef in_device_properties,
                       AudioDeviceID* out_device) {
            *out_device = kAggregateDeviceId;
            return noErr;
          });
      // Add call expectation for AudioDeviceCreateIOProcID. Store the callback
      // to be able to simulate audio capture callbacks.
      EXPECT_CALL(mock_catap_api(), AudioDeviceCreateIOProcID)
          .Times(1)
          .WillOnce([this](AudioDeviceID in_device, AudioDeviceIOProc proc,
                           void* in_client_data,
                           AudioDeviceIOProcID* out_proc_id) {
            EXPECT_EQ(in_device, kAggregateDeviceId);
            audio_proc_ = proc;
            EXPECT_EQ(in_client_data, stream_);
            *out_proc_id = kTapIoProcId;
            return noErr;
          });

      // The following two calls are done when probing the tap, which is
      // enabled by default.
      EXPECT_CALL(mock_catap_api(), AudioObjectGetPropertyData)
          .Times(1)
          .WillOnce(testing::Return(noErr));
      EXPECT_CALL(mock_catap_api(), AudioObjectSetPropertyData)
          .Times(1)
          .WillOnce(testing::Return(with_permissions ? noErr : -1));

      // Initialize the stream.
      return stream_->Open();
    }
    return AudioInputStream::OpenOutcome::kFailed;
  }

  void TearDown() override {
    if (@available(macOS 14.2, *)) {
      if (!stream_) {
        return;
      }
      EXPECT_CALL(mock_catap_api(), AudioDeviceDestroyIOProcID)
          .Times(1)
          .WillOnce(
              [](AudioDeviceID in_device, AudioDeviceIOProcID in_proc_id) {
                EXPECT_EQ(in_device, kAggregateDeviceId);
                EXPECT_EQ(in_proc_id, kTapIoProcId);
                return noErr;
              });
      EXPECT_CALL(mock_catap_api(), AudioHardwareDestroyAggregateDevice)
          .Times(1)
          .WillOnce([](AudioDeviceID in_device) {
            EXPECT_EQ(in_device, kAggregateDeviceId);
            return noErr;
          });
      EXPECT_CALL(mock_catap_api(), AudioHardwareDestroyProcessTap)
          .Times(1)
          .WillOnce([](AudioObjectID in_tap) {
            EXPECT_EQ(in_tap, kTap);
            return noErr;
          });
      stream_->Stop();
      stream_->Close();
      mock_catap_api_ = nullptr;
      stream_.ClearAndDelete();
    }
  }

  API_AVAILABLE(macos(14.2)) MockCatapApi& mock_catap_api() {
    return static_cast<MockCatapApi&>(*mock_catap_api_);
  }

 protected:
  raw_ptr<AudioInputStream> stream_;
  raw_ptr<CatapApi> mock_catap_api_;
  MockAudioInputCallback mock_callback_;
  AudioDeviceIOProc audio_proc_;
};

TEST_F(CatapAudioInputStreamTest, CreateAndInitializeWithPermissions) {
  if (@available(macOS 14.2, *)) {
    EXPECT_EQ(CreateAndOpenStream(/*with_permissions=*/true),
              AudioInputStream::OpenOutcome::kSuccess);
  }
}

TEST_F(CatapAudioInputStreamTest, CreateAndFailToInitializeWithoutPermissions) {
  if (@available(macOS 14.2, *)) {
    EXPECT_EQ(CreateAndOpenStream(/*with_permissions=*/false),
              AudioInputStream::OpenOutcome::kFailed);
  }
}

TEST_F(CatapAudioInputStreamTest, CaptureSomeAudioData) {
  if (@available(macOS 14.2, *)) {
    EXPECT_EQ(CreateAndOpenStream(/*with_permissions=*/true),
              AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_CALL(mock_callback_,
                OnData(testing::_, testing::_, testing::_, testing::_))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(mock_catap_api(), AudioDeviceStart)
        .Times(1)
        .WillOnce([](AudioDeviceID in_device, AudioDeviceIOProcID in_proc_id) {
          EXPECT_EQ(in_device, kAggregateDeviceId);
          EXPECT_EQ(in_proc_id, kTapIoProcId);
          return noErr;
        });
    stream_->Start(&mock_callback_);
    ASSERT_NE(audio_proc_, nullptr);
    // Simulate a call to `audio_proc_` with some data.
    const AudioTimeStamp* in_now = nullptr;
    const uint32_t data_byte_size =
        kCatapLoopbackFramesPerBuffer * sizeof(Float32) * 2;
    std::vector<uint8_t> data_buffer(data_byte_size);

    AudioBufferList input_data;
    input_data.mNumberBuffers = 1;
    AudioBuffer& input_buffer = input_data.mBuffers[0];
    input_buffer.mNumberChannels = 2;
    input_buffer.mDataByteSize = data_byte_size;
    input_buffer.mData = data_buffer.data();

    AudioTimeStamp input_time;
    input_time.mFlags = kAudioTimeStampHostTimeValid;
    input_time.mHostTime = mach_absolute_time();
    AudioBufferList* output_data = nullptr;
    const AudioTimeStamp* output_time = nullptr;

    audio_proc_(0, in_now, &input_data, &input_time, output_data, output_time,
                stream_);
    EXPECT_CALL(mock_catap_api(), AudioDeviceStop)
        .Times(1)
        .WillOnce([](AudioDeviceID in_device, AudioDeviceIOProcID in_proc_id) {
          EXPECT_EQ(in_device, kAggregateDeviceId);
          EXPECT_EQ(in_proc_id, kTapIoProcId);
          return noErr;
        });
    stream_->Stop();
  }
}
}  // namespace media
