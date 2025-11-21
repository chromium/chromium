// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/audio/mac/catap_audio_input_stream.h"

#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#import <Foundation/Foundation.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "media/audio/application_loopback_device_helper.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/catap_api.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

bool operator==(const AudioObjectPropertyAddress& x,
                const AudioObjectPropertyAddress& y) {
  return x.mSelector == y.mSelector && x.mScope == y.mScope &&
         x.mElement == y.mElement;
}

namespace {
void LogToStderr(const std::string& message) {
  LOG(INFO) << message;
}

constexpr int kNumberOfChannelsMono = 1;
constexpr int kNumberOfChannelsStereo = 2;

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

constexpr AudioObjectID kDefaultOutputId = 29;
constexpr std::string kDefaultOutputUID = "default_output_uid";

API_AVAILABLE(macos(14.2))
const CatapAudioInputStream::AudioDeviceIds kDefaultOutputIds(
    kDefaultOutputId,
    kDefaultOutputUID);

API_AVAILABLE(macos(14.2))
const CatapAudioInputStream::AudioDeviceIds kNoDefaultOutputIds;

const AudioObjectPropertyAddress kDeviceIsAliveAddress = {
    kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kDefaultOutputDevicePropertyAddress = {
    kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kVirtualFormatAddress = {
    kAudioStreamPropertyVirtualFormat, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kSampleRateAddress = {
    kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kBufferFrameSizeAddress = {
    kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kProcessObjectListAddress = {
    kAudioHardwarePropertyProcessObjectList, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kProcessPidAddress = {
    kAudioProcessPropertyPID, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress kTapDescriptionAddress = {
    kAudioTapPropertyDescription, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

// Fake for the AudioInputCallback.
class FakeAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  void OnData(const AudioBus* bus,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    ++on_data_call_count_;
    last_capture_time_ = capture_time;
    last_number_of_channels_ = bus->channels();
    last_number_of_frames_ = bus->frames();
  }

  void OnError() override { on_error_call_count_++; }

  int on_data_call_count() const { return on_data_call_count_; }
  int on_error_call_count() const { return on_error_call_count_; }
  base::TimeTicks last_capture_time() const { return last_capture_time_; }
  int last_number_of_channels() const { return last_number_of_channels_; }
  int last_number_of_frames() const { return last_number_of_frames_; }

 private:
  int on_data_call_count_ = 0;
  int on_error_call_count_ = 0;
  base::TimeTicks last_capture_time_;
  int last_number_of_channels_ = 0;
  int last_number_of_frames_ = 0;
};

// Fake for all CoreAudio API calls.
//
// This class provides a fake implementation of the `CatapApi` interface for use
// in unit tests. It allows tests to simulate the behavior of the CoreAudio API
// and verify that `CatapAudioInputStream` interacts with it correctly.
//
// The typical usage pattern is:
// 1. Create an instance of `FakeCatapApi`.
// 2. (Optional) Configure its behavior by setting the public member variables
//    in the "Public properties that can be modified by the tests" section.
//    For example, set `with_permissions` to `false` to simulate a scenario
//    where the user has not granted screen capture permissions.
// 3. Pass the `FakeCatapApi` instance to the `CatapAudioInputStream` under
//    test.
// 4. Run the code under test.
// 5. Inspect the public member variables in the "Variables that can be
//    inspected by the tests" section to verify that the `CatapAudioInputStream`
//    called the correct `CatapApi` functions with the expected parameters.
//    For example, check that `created_aggregate_device` is `true`.
class FakeCatapApi : public CatapApi {
 public:
  FakeCatapApi() = default;
  ~FakeCatapApi() override = default;

  // CatapApi:
  OSStatus AudioHardwareCreateAggregateDevice(
      CFDictionaryRef in_device_properties,
      AudioDeviceID* out_device) override {
    if (should_fail_create_aggregate_device) {
      return -1;
    }
    created_aggregate_device = true;
    *out_device = kAggregateDeviceId;
    ++create_aggregate_device_count;
    return noErr;
  }
  OSStatus AudioDeviceCreateIOProcID(
      AudioDeviceID in_device,
      AudioDeviceIOProc proc,
      void* in_client_data,
      AudioDeviceIOProcID* out_proc_id) override {
    io_proc_id_created_for_device = in_device;
    audio_proc = proc;
    client_data = in_client_data;
    *out_proc_id = kTapIoProcId;
    return noErr;
  }
  OSStatus AudioObjectGetPropertyDataSize(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32* ioDataSize) override {
    if (*in_address == kProcessObjectListAddress) {
      *ioDataSize = process_audio_devices.size() * sizeof(AudioDeviceID);
      return noErr;
    }
    return noErr;
  }
  OSStatus AudioObjectGetPropertyData(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32* ioDataSize,
      void* outData) override {
    if (*in_address == kProcessObjectListAddress) {
      *ioDataSize = process_audio_devices.size() * sizeof(AudioDeviceID);
      base::span UNSAFE_BUFFERS(
          device_ids(reinterpret_cast<AudioDeviceID*>(outData),
                     process_audio_devices.size()));
      for (size_t i = 0; i < process_audio_devices.size(); ++i) {
        device_ids[i] = process_audio_devices[i];
      }
      return noErr;
    }
    if (*in_address == kProcessPidAddress) {
      auto it = process_pids.find(in_object_id);
      if (it != process_pids.end()) {
        *reinterpret_cast<pid_t*>(outData) = it->second;
        *ioDataSize = sizeof(pid_t);
        return noErr;
      }
      return -1;
    }
    if (*in_address == kDeviceIsAliveAddress) {
      *reinterpret_cast<UInt32*>(outData) = device_is_alive;
      *ioDataSize = sizeof(UInt32);
      return noErr;
    }
    if (*in_address == kSampleRateAddress) {
      if (!last_set_sample_rate.has_value()) {
        return -1;
      }
      *reinterpret_cast<Float64*>(outData) = *last_set_sample_rate;
      *ioDataSize = sizeof(Float64);
      return noErr;
    }
    if (*in_address == kTapDescriptionAddress) {
      // The tap description is returned as an ARC-managed object, so we need to
      // create a retained copy.
      *reinterpret_cast<void**>(outData) =
          (__bridge_retained void*)last_tap_description;
      *ioDataSize = sizeof(CATapDescription*);
      return noErr;
    }
    if (*in_address == kVirtualFormatAddress) {
      AudioStreamBasicDescription* stream_format =
          reinterpret_cast<AudioStreamBasicDescription*>(outData);
      stream_format->mChannelsPerFrame = number_of_device_channels;
      *ioDataSize = sizeof(AudioStreamBasicDescription);
      return noErr;
    }
    return noErr;
  }
  OSStatus AudioObjectSetPropertyData(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32 in_data_size,
      const void* in_data) override {
    if (*in_address == kSampleRateAddress) {
      last_set_sample_rate = *reinterpret_cast<const Float64*>(in_data);
      ++set_sample_rate_count;
    } else if (*in_address == kBufferFrameSizeAddress) {
      last_set_frames_per_buffer = *reinterpret_cast<const UInt32*>(in_data);
      ++set_frames_per_buffer_count;
    } else if (*in_address == kTapDescriptionAddress) {
      ++set_tap_description_count;
      return with_permissions ? noErr : -1;
    } else {
      return -1;
    }
    return noErr;
  }

  API_AVAILABLE(macos(14.2))
  OSStatus AudioHardwareCreateProcessTap(CATapDescription* in_description,
                                         AudioObjectID* out_tap) override {
    last_tap_description = in_description;
    *out_tap = kTap;
    return noErr;
  }
  OSStatus AudioDeviceStart(AudioDeviceID in_device,
                            AudioDeviceIOProcID in_proc_id) override {
    started_device = in_device;
    started_proc_id = in_proc_id;
    return noErr;
  }
  OSStatus AudioDeviceStop(AudioDeviceID in_device,
                           AudioDeviceIOProcID in_proc_id) override {
    stopped_device = in_device;
    stopped_proc_id = in_proc_id;
    return noErr;
  }
  OSStatus AudioDeviceDestroyIOProcID(AudioDeviceID in_device,
                                      AudioDeviceIOProcID in_proc_id) override {
    destroyed_io_proc_id_for_device = in_device;
    destroyed_io_proc_id = in_proc_id;
    client_data = nullptr;
    return noErr;
  }
  OSStatus AudioHardwareDestroyAggregateDevice(
      AudioDeviceID in_device) override {
    destroyed_aggregate_device = in_device;
    return noErr;
  }
  OSStatus AudioHardwareDestroyProcessTap(AudioObjectID in_tap) override {
    destroyed_process_tap = in_tap;
    return noErr;
  }

  OSStatus AudioObjectAddPropertyListenerBlock(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      dispatch_queue_t in_dispatch_queue,
      AudioObjectPropertyListenerBlock in_listener) override {
    property_listener_block = in_listener;
    last_added_property_listener_address = *in_address;
    ++property_listener_block_count;
    return noErr;
  }
  OSStatus AudioObjectRemovePropertyListenerBlock(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      dispatch_queue_t in_dispatch_queue,
      AudioObjectPropertyListenerBlock in_listener) override {
    last_removed_property_listener_address = *in_address;
    --property_listener_block_count;

    if (property_listener_block_count == 0) {
      property_listener_block = nil;
    }
    return noErr;
  }

  // Public properties that can be modified by the tests.

  // If `true`, `AudioObjectSetPropertyData()` will return `noErr` when setting
  // the tap description. Otherwise it will return an error, which simulates
  // that the user has not given screen capture permissions.
  bool with_permissions = true;
  // If `true`, the fake device will be reported as "alive". If `false` it will
  // be reported as not alive. This is used to simulate device disconnections.
  bool device_is_alive = true;
  // Used when the `AudioObjectGetPropertyData()` is called to get the stream
  // format of the default output device. Can be overriden to simulate a mono
  // device.
  int number_of_device_channels = kNumberOfChannelsStereo;
  // Used to simulate the list of process audio device IDs that belong to
  // different processes.
  std::vector<AudioDeviceID> process_audio_devices;
  // The key is the device ID and the value is the process ID. This is used to
  // map device IDs to process IDs.
  std::map<AudioDeviceID, pid_t> process_pids;

  // Variables that can be inspected by the tests.
  // The following variables are set when the corresponding `CatapApi` function
  // is called. Tests can inspect them to verify that the code under test calls
  // the correct functions with the expected parameters.
  bool created_aggregate_device = false;
  AudioDeviceID io_proc_id_created_for_device = 0;
  AudioDeviceID started_device = 0;
  AudioDeviceIOProcID started_proc_id = nullptr;
  AudioDeviceID stopped_device = 0;
  AudioDeviceIOProcID stopped_proc_id = nullptr;
  AudioDeviceID destroyed_io_proc_id_for_device = 0;
  AudioDeviceIOProcID destroyed_io_proc_id = nullptr;
  AudioDeviceID destroyed_aggregate_device = 0;
  AudioObjectID destroyed_process_tap = 0;

  // The last sample rate that was set with `AudioObjectSetPropertyData()`. This
  // is also the value that is returned by `AudioObjectGetPropertyData()` when
  // the sample rate is requested.
  std::optional<Float64> last_set_sample_rate;
  // The last frames per buffer that was set with
  // `AudioObjectSetPropertyData()`.
  std::optional<UInt32> last_set_frames_per_buffer;
  // The last tap description passed to `AudioHardwareCreateProcessTap()`.
  // It's an ARC-managed object that is also returned by
  // `AudioObjectGetPropertyData()` when a tap description is requested.
  CATapDescription* last_tap_description = nullptr;
  // The callback function pointer that was passed to
  // `AudioDeviceCreateIOProcID()`.
  AudioDeviceIOProc audio_proc = nullptr;
  // The client data that was passed to `AudioDeviceCreateIOProcID()`.
  raw_ptr<void> client_data = nullptr;
  // Counters for how many times a specific property was set.
  int set_sample_rate_count = 0;
  int set_frames_per_buffer_count = 0;
  int set_tap_description_count = 0;
  // Counter for how many property listener blocks are currently active.
  // This should be 0 when the stream is closed.
  int property_listener_block_count = 0;
  // Counter for how many times an aggregate device is created.
  int create_aggregate_device_count = 0;
  // The last property listener block that was added with
  // `AudioObjectAddPropertyListenerBlock()`.
  AudioObjectPropertyListenerBlock property_listener_block = nil;
  // The address of the last property listener that was added with
  // `AudioObjectAddPropertyListenerBlock()`.
  AudioObjectPropertyAddress last_added_property_listener_address;
  // The address of the last property listener that was removed with
  // `AudioObjectRemovePropertyListenerBlock()`.
  AudioObjectPropertyAddress last_removed_property_listener_address;
  // If the call to `AudioHardwareCreateAggregateDevice()` will fail.
  bool should_fail_create_aggregate_device = false;
};

}  // namespace

class CatapAudioInputStreamTest : public testing::Test {
 public:
  CatapAudioInputStreamTest() = default;
  ~CatapAudioInputStreamTest() override = default;

  API_AVAILABLE(macos(14.2))
  void CreateStream(bool with_permissions = true,
                    const std::string& device_id =
                        media::AudioDeviceDescription::kLoopbackInputDeviceId) {
    CreateStream(with_permissions, device_id, kDefaultOutputIds);
  }

  API_AVAILABLE(macos(14.2))
  void CreateStream(
      bool with_permissions,
      const std::string& device_id,
      const CatapAudioInputStream::AudioDeviceIds default_device_ids) {
    auto fake_catap_api_object = std::make_unique<FakeCatapApi>();
    fake_catap_api_object->with_permissions = with_permissions;
    fake_catap_api_ = fake_catap_api_object.get();

    auto returning_lambda = [](const CatapAudioInputStream::AudioDeviceIds& ids)
        -> CatapAudioInputStream::AudioDeviceIds { return ids; };
    auto device_id_callback =
        base::BindRepeating(returning_lambda, default_device_ids);

    stream_ = CreateCatapAudioInputStreamForTesting(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        ChannelLayoutConfig::Stereo(), kLoopbackSampleRate,
                        kCatapLoopbackDefaultFramesPerBuffer),
        device_id, base::BindRepeating(LogToStderr), base::DoNothing(),
        std::move(fake_catap_api_object), std::move(device_id_callback));
    EXPECT_TRUE(stream_);
  }

  API_AVAILABLE(macos(14.2))
  void CheckSuccessfulOpen() {
      EXPECT_EQ(fake_catap_api()->io_proc_id_created_for_device,
                kAggregateDeviceId);
      EXPECT_EQ(fake_catap_api()->set_sample_rate_count, 1);
      EXPECT_FLOAT_EQ(fake_catap_api()->last_set_sample_rate.value(),
                      kLoopbackSampleRate);
      EXPECT_EQ(fake_catap_api()->set_frames_per_buffer_count, 1);
      EXPECT_FLOAT_EQ(fake_catap_api()->last_set_frames_per_buffer.value(),
                      kCatapLoopbackDefaultFramesPerBuffer);
      EXPECT_EQ(fake_catap_api()->set_tap_description_count, 1);
  }

  API_AVAILABLE(macos(14.2))
  void EnsureStreamIsActive() {
      ASSERT_NE(fake_catap_api()->audio_proc, nullptr);
      const AudioTimeStamp* in_now = nullptr;
      const uint32_t data_byte_size =
          kCatapLoopbackDefaultFramesPerBuffer * sizeof(Float32) *
          fake_catap_api()->number_of_device_channels;
      std::vector<uint8_t> data_buffer(data_byte_size);

      AudioBufferList input_data;
      input_data.mNumberBuffers = 1;
      AudioBuffer& input_buffer = input_data.mBuffers[0];
      input_buffer.mNumberChannels =
          fake_catap_api()->number_of_device_channels;
      input_buffer.mDataByteSize = data_byte_size;
      input_buffer.mData = data_buffer.data();

      AudioTimeStamp input_time;
      input_time.mFlags = kAudioTimeStampHostTimeValid;
      input_time.mHostTime = mach_absolute_time();
      AudioBufferList* output_data = nullptr;
      const AudioTimeStamp* output_time = nullptr;

      fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                   output_data, output_time,
                                   fake_catap_api()->client_data);
      EXPECT_GE(fake_callback_.on_data_call_count(), 1);
  }

  void TearDown() override {
    if (@available(macOS 14.2, *)) {
      if (!stream_) {
        return;
      }
      stream_->Stop();
      stream_->Close();
      EXPECT_EQ(fake_catap_api_->destroyed_io_proc_id_for_device,
                kAggregateDeviceId);
      EXPECT_EQ(fake_catap_api_->destroyed_io_proc_id, kTapIoProcId);
      EXPECT_EQ(fake_catap_api_->destroyed_aggregate_device,
                kAggregateDeviceId);
      EXPECT_EQ(fake_catap_api_->destroyed_process_tap, kTap);
      fake_catap_api_ = nullptr;
      stream_.ClearAndDelete();
    }
  }

  API_AVAILABLE(macos(14.2)) FakeCatapApi* fake_catap_api() {
    return fake_catap_api_;
  }

 protected:
  raw_ptr<AudioInputStream> stream_;
  raw_ptr<FakeCatapApi> fake_catap_api_;
  FakeAudioInputCallback fake_callback_;
};

TEST_F(CatapAudioInputStreamTest, CreateAndInitializeWithPermissions) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);

    CheckSuccessfulOpen();
    EXPECT_EQ(std::string(base::SysNSStringToUTF8(
                  [fake_catap_api()->last_tap_description deviceUID])),
              kDefaultOutputUID);
    EXPECT_EQ([[fake_catap_api()->last_tap_description stream] intValue], 0);
    EXPECT_EQ([fake_catap_api()->last_tap_description isMuted], CATapUnmuted);
  }
}

TEST_F(CatapAudioInputStreamTest, CreateAndFailToInitializeWithoutPermissions) {
  if (@available(macOS 14.2, *)) {
    CreateStream(/*with_permissions=*/false);
    EXPECT_EQ(stream_->Open(),
              AudioInputStream::OpenOutcome::kFailedSystemPermissions);
    EXPECT_EQ(fake_catap_api()->set_tap_description_count, 1);
  }
}

TEST_F(CatapAudioInputStreamTest, DoubleOpenResultsInkAlreadyOpen) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kAlreadyOpen);
  }
}

TEST_F(CatapAudioInputStreamTest, CaptureSomeAudioData) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);
    EXPECT_EQ(fake_catap_api()->started_device, kAggregateDeviceId);
    EXPECT_EQ(fake_catap_api()->started_proc_id, kTapIoProcId);

    ASSERT_NE(fake_catap_api()->audio_proc, nullptr);
    // Simulate a call to `audio_proc` with some data.
    const AudioTimeStamp* in_now = nullptr;
    const uint32_t data_byte_size = kCatapLoopbackDefaultFramesPerBuffer *
                                    sizeof(Float32) * kNumberOfChannelsStereo;
    ;
    std::vector<uint8_t> data_buffer(data_byte_size);

    AudioBufferList input_data;
    input_data.mNumberBuffers = 1;
    AudioBuffer& input_buffer = input_data.mBuffers[0];
    input_buffer.mNumberChannels = kNumberOfChannelsStereo;
    input_buffer.mDataByteSize = data_byte_size;
    input_buffer.mData = data_buffer.data();

    AudioTimeStamp input_time;
    input_time.mFlags = kAudioTimeStampHostTimeValid;
    input_time.mHostTime = mach_absolute_time();
    AudioBufferList* output_data = nullptr;
    const AudioTimeStamp* output_time = nullptr;

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);
    EXPECT_GE(fake_callback_.on_data_call_count(), 1);

    stream_->Stop();
    EXPECT_EQ(fake_catap_api()->stopped_device, kAggregateDeviceId);
    EXPECT_EQ(fake_catap_api()->stopped_proc_id, kTapIoProcId);
  }
}

TEST_F(CatapAudioInputStreamTest, CaptureSomeAudioDataMissingHostTime) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);
    EXPECT_EQ(fake_catap_api()->started_device, kAggregateDeviceId);
    EXPECT_EQ(fake_catap_api()->started_proc_id, kTapIoProcId);

    ASSERT_NE(fake_catap_api()->audio_proc, nullptr);
    // Simulate a call to `audio_proc_` with some data.
    const AudioTimeStamp* in_now = nullptr;
    const uint32_t data_byte_size = kCatapLoopbackDefaultFramesPerBuffer *
                                    sizeof(Float32) * kNumberOfChannelsStereo;
    std::vector<uint8_t> data_buffer(data_byte_size);

    AudioBufferList input_data;
    input_data.mNumberBuffers = 1;
    AudioBuffer& input_buffer = input_data.mBuffers[0];
    input_buffer.mNumberChannels = kNumberOfChannelsStereo;
    input_buffer.mDataByteSize = data_byte_size;
    input_buffer.mData = data_buffer.data();

    AudioTimeStamp input_time;
    input_time.mFlags = kAudioTimeStampHostTimeValid;
    input_time.mHostTime = mach_absolute_time();
    AudioBufferList* output_data = nullptr;
    const AudioTimeStamp* output_time = nullptr;

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);

    // Simulate one more captured frame without a host timestamp. Expect the
    // timestamp of the next OnData() call to be incremented by one buffer
    // duration.
    base::TimeTicks previous_capture_timestamp =
        base::TimeTicks::FromMachAbsoluteTime(input_time.mHostTime);
    input_time.mFlags = 0;
    input_time.mHostTime = 0;

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);

    base::TimeDelta kExpectedBufferDuration = base::Milliseconds(
        1000 * kCatapLoopbackDefaultFramesPerBuffer / kLoopbackSampleRate);
    base::TimeDelta kCaptureTimeTolerance = base::Milliseconds(1);
    EXPECT_LE((fake_callback_.last_capture_time() - previous_capture_timestamp -
               kExpectedBufferDuration)
                  .magnitude(),
              kCaptureTimeTolerance);

    stream_->Stop();
    EXPECT_EQ(fake_catap_api()->stopped_device, kAggregateDeviceId);
    EXPECT_EQ(fake_catap_api()->stopped_proc_id, kTapIoProcId);
  }
}

TEST_F(CatapAudioInputStreamTest, LoopbackWithoutChromeId) {
  if (@available(macOS 14.2, *)) {
    CreateStream(
        /*with_permissions=*/true,
        /*device_id=*/media::AudioDeviceDescription::kLoopbackWithoutChromeId);

    // Arbitrary number of CoreAudio process audio device IDs to be returned by
    // GetProcessAudioDeviceIds.
    constexpr AudioDeviceID kChromeProcessDeviceId = 1;
    constexpr AudioDeviceID kOtherProcessDeviceId = 2;
    fake_catap_api()->process_audio_devices = {kChromeProcessDeviceId,
                                               kOtherProcessDeviceId};
    fake_catap_api()->process_pids[kChromeProcessDeviceId] = getpid();
    fake_catap_api()->process_pids[kOtherProcessDeviceId] = getpid() + 1;

    // Initialize the stream.
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);

    // The device with the current process ID should be excluded.
    std::set<AudioObjectID> device_ids_to_exclude = {kChromeProcessDeviceId};
    EXPECT_EQ([fake_catap_api()->last_tap_description processes].count,
              device_ids_to_exclude.size());
    // For system audio loopback, the `processes` list contains the specific
    // `AudioObjects` we want to exclude from capture. Therefore, the
    // `exclusive` flag must be true.
    EXPECT_TRUE([fake_catap_api()->last_tap_description isExclusive]);
    for (NSNumber* device_id_number in
         [fake_catap_api()->last_tap_description processes]) {
      EXPECT_TRUE(device_ids_to_exclude.count(
          static_cast<AudioObjectID>([device_id_number intValue])));
    }
  }
}

TEST_F(CatapAudioInputStreamTest, ApplicationLoopback) {
  if (@available(macOS 14.2, *)) {
    base::ProcessId process_id = getpid();
    CreateStream(
        /*with_permissions=*/true,
        /*device_id=*/media::CreateApplicationLoopbackDeviceId(process_id));

    // Arbitrary number of CoreAudio process audio device IDs to be returned by
    // GetProcessAudioDeviceIds.
    constexpr AudioDeviceID kProcessFirstDeviceId = 1;
    constexpr AudioDeviceID kProcessSecondDeviceId = 2;
    constexpr AudioDeviceID kOtherProcessDeviceId = 3;
    fake_catap_api()->process_audio_devices = {
        kProcessFirstDeviceId, kProcessSecondDeviceId, kOtherProcessDeviceId};
    fake_catap_api()->process_pids[kProcessFirstDeviceId] = process_id;
    fake_catap_api()->process_pids[kProcessSecondDeviceId] = process_id;
    fake_catap_api()->process_pids[kOtherProcessDeviceId] = process_id + 1;

    // Initialize the stream.
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);

    // The device with the current process ID should be included.
    std::set<AudioObjectID> device_ids_to_include = {kProcessFirstDeviceId,
                                                     kProcessSecondDeviceId};
    EXPECT_EQ([fake_catap_api()->last_tap_description processes].count,
              device_ids_to_include.size());
    // For application loopback, the `processes` list contains the specific
    // `AudioObjects` we want to capture (include). Therefore, the `exclusive`
    // flag must be false.
    EXPECT_FALSE([fake_catap_api()->last_tap_description isExclusive]);
    for (NSNumber* device_id_number in
         [fake_catap_api()->last_tap_description processes]) {
      EXPECT_TRUE(device_ids_to_include.count(
          static_cast<AudioObjectID>([device_id_number intValue])));
    }

    // In application loopback we capture all output devices. In that case
    // Device UID and stream should not have been set.
    EXPECT_EQ([fake_catap_api()->last_tap_description deviceUID], nullptr);
    EXPECT_EQ([fake_catap_api()->last_tap_description stream], nullptr);
  }
}

TEST_F(CatapAudioInputStreamTest, LoopbackWithMuteDevice) {
  if (@available(macOS 14.2, *)) {
    CreateStream(
        /*with_permissions=*/true,
        /*device_id=*/media::AudioDeviceDescription::kLoopbackWithMuteDeviceId);
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    CheckSuccessfulOpen();
    EXPECT_EQ([fake_catap_api()->last_tap_description isMuted], CATapMuted);
  }
}

TEST_F(CatapAudioInputStreamTest, LoopbackWithAllDevices) {
  if (@available(macOS 14.2, *)) {
    CreateStream(
        /*with_permissions=*/true,
        /*device_id=*/media::AudioDeviceDescription::kLoopbackAllDevicesId);
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    CheckSuccessfulOpen();
    // Device UID and stream not set indicates that we're capturing all
    // devices.
    EXPECT_EQ([fake_catap_api()->last_tap_description deviceUID], nullptr);
    EXPECT_EQ([fake_catap_api()->last_tap_description stream], nullptr);
  }
}

TEST_F(CatapAudioInputStreamTest, ErrorIfDeviceIsAliveChanges) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);

    // Simulate that "device is alive" is changed to false.
    fake_catap_api()->device_is_alive = false;

    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);
    fake_catap_api()->property_listener_block(1, &kDeviceIsAliveAddress);
    EXPECT_EQ(fake_callback_.on_error_call_count(), 1);
  }
}

TEST_F(CatapAudioInputStreamTest, NoErrorIfDefaultOutputDeviceChanges) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);

    // Simulate a change of default output device.
    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);
    fake_catap_api()->property_listener_block(
        1, &kDefaultOutputDevicePropertyAddress);
    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);
  }
}

TEST_F(CatapAudioInputStreamTest, PropertyListenerRemovedOnClose) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);

    EXPECT_GT(fake_catap_api()->property_listener_block_count, 0);
    stream_->Close();
    EXPECT_EQ(fake_catap_api()->property_listener_block_count, 0);
    // Prevent TearDown from calling Close() again on a closed stream.
    fake_catap_api_ = nullptr;
    stream_.ClearAndDelete();
  }
}

TEST_F(CatapAudioInputStreamTest, ChannelCountChangeIsIgnored) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);

    ASSERT_NE(fake_catap_api()->audio_proc, nullptr);

    // Simulate a call to `audio_proc` with correct data.
    const AudioTimeStamp* in_now = nullptr;
    const uint32_t data_byte_size = kCatapLoopbackDefaultFramesPerBuffer *
                                    sizeof(Float32) * kNumberOfChannelsStereo;
    std::vector<uint8_t> data_buffer(data_byte_size);

    AudioBufferList input_data;
    input_data.mNumberBuffers = 1;
    AudioBuffer& input_buffer = input_data.mBuffers[0];
    input_buffer.mNumberChannels = kNumberOfChannelsStereo;
    input_buffer.mDataByteSize = data_byte_size;
    input_buffer.mData = data_buffer.data();

    AudioTimeStamp input_time;
    input_time.mFlags = kAudioTimeStampHostTimeValid;
    input_time.mHostTime = mach_absolute_time();
    AudioBufferList* output_data = nullptr;
    const AudioTimeStamp* output_time = nullptr;

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);
    EXPECT_EQ(fake_callback_.on_data_call_count(), 1);
    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);

    // Now simulate a call with a different number of channels.
    input_buffer.mNumberChannels = kNumberOfChannelsMono;
    input_time.mHostTime = mach_absolute_time();

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);

    // OnData should not be called again, and no error should be reported.
    EXPECT_EQ(fake_callback_.on_data_call_count(), 1);
    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);

    stream_->Stop();
  }
}

TEST_F(CatapAudioInputStreamTest, FramesPerBufferChangeIsIgnored) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);

    ASSERT_NE(fake_catap_api()->audio_proc, nullptr);

    // Simulate a call to `audio_proc` with correct data.
    const AudioTimeStamp* in_now = nullptr;
    const uint32_t data_byte_size = kCatapLoopbackDefaultFramesPerBuffer *
                                    sizeof(Float32) * kNumberOfChannelsStereo;
    std::vector<uint8_t> data_buffer(data_byte_size);

    AudioBufferList input_data;
    input_data.mNumberBuffers = 1;
    AudioBuffer& input_buffer = input_data.mBuffers[0];
    input_buffer.mNumberChannels = kNumberOfChannelsStereo;
    input_buffer.mDataByteSize = data_byte_size;
    input_buffer.mData = data_buffer.data();

    AudioTimeStamp input_time;
    input_time.mFlags = kAudioTimeStampHostTimeValid;
    input_time.mHostTime = mach_absolute_time();
    AudioBufferList* output_data = nullptr;
    const AudioTimeStamp* output_time = nullptr;

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);
    EXPECT_EQ(fake_callback_.on_data_call_count(), 1);
    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);

    // Now simulate a call with a different number of frames per buffer.
    input_buffer.mDataByteSize = data_byte_size / 2;
    input_time.mHostTime = mach_absolute_time();

    fake_catap_api()->audio_proc(0, in_now, &input_data, &input_time,
                                 output_data, output_time,
                                 fake_catap_api()->client_data);

    // OnData should not be called again, and no error should be reported.
    EXPECT_EQ(fake_callback_.on_data_call_count(), 1);
    EXPECT_EQ(fake_callback_.on_error_call_count(), 0);

    stream_->Stop();
  }
}

TEST_F(CatapAudioInputStreamTest, ReopensOnDeviceChangeForOpenStream) {
  if (@available(macOS 14.2, *)) {
    int expected_create_aggregate_device_count = 0;
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    ++expected_create_aggregate_device_count;

    // An aggregated device is created once in Open()
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // Check if catap was recreated on default device change.
    fake_catap_api()->property_listener_block(
        1, &kDefaultOutputDevicePropertyAddress);
    ++expected_create_aggregate_device_count;
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    stream_->Start(&fake_callback_);

    // Check if the catap stream still calls the OnData() callback.
    EnsureStreamIsActive();
  }
}

TEST_F(CatapAudioInputStreamTest, RestartsOnDeviceChangeForStartedStream) {
  if (@available(macOS 14.2, *)) {
    int expected_create_aggregate_device_count = 0;
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    ++expected_create_aggregate_device_count;

    // An aggregated device is created once in Open()
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    stream_->Start(&fake_callback_);

    // Check if catap was recreated on default device change.
    fake_catap_api()->property_listener_block(
        1, &kDefaultOutputDevicePropertyAddress);
    ++expected_create_aggregate_device_count;
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // Check if the catap stream still calls the OnData() callback.
    EnsureStreamIsActive();
  }
}

TEST_F(CatapAudioInputStreamTest, ReopensOnSamplerateChange) {
  if (@available(macOS 14.2, *)) {
    int expected_create_aggregate_device_count = 0;
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    ++expected_create_aggregate_device_count;

    // An aggregated device is created once in Open()
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // No change on sample rate notifications if sample rate haven't changed.
    fake_catap_api()->property_listener_block(1, &kSampleRateAddress);
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // Check if catap was recreated on real sampe rate change.
    fake_catap_api()->last_set_sample_rate = 16000;
    fake_catap_api()->property_listener_block(1, &kSampleRateAddress);
    ++expected_create_aggregate_device_count;
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);
  }
}

TEST_F(CatapAudioInputStreamTest, RestartFailingForStartedStream) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    stream_->Start(&fake_callback_);

    // Trigger a restart that fails
    fake_catap_api()->should_fail_create_aggregate_device = true;
    fake_catap_api()->property_listener_block(
        1, &kDefaultOutputDevicePropertyAddress);

    // Failed restart should trigger an OnError() call.
    EXPECT_EQ(fake_callback_.on_error_call_count(), 1);

    stream_->Stop();
    stream_->Close();
    fake_catap_api_ = nullptr;
    stream_.ClearAndDelete();
  }
}

TEST_F(CatapAudioInputStreamTest, NoDefaultDevice) {
  if (@available(macOS 14.2, *)) {
    // Not providing an UID for default device should cause Open() to fail with
    // unless device id == kLoopbackAllDevicesId.
    CreateStream(
        /*with_permissions=*/true,
        /*device_id=*/media::AudioDeviceDescription::kLoopbackInputDeviceId,
        /*default_device_ids=*/kNoDefaultOutputIds);
    EXPECT_NE(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count, 0);
    stream_->Close();
    fake_catap_api_ = nullptr;
    stream_.ClearAndDelete();

    // If device id == kLoopbackAllDevicesId it doesn't need an UID for default
    // device, and kSuccess from Open() is expected.
    CreateStream(
        /*with_permissions=*/true,
        /*device_id=*/media::AudioDeviceDescription::kLoopbackAllDevicesId,
        /*default_device_ids=*/kNoDefaultOutputIds);
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count, 1);
  }
}

TEST_F(CatapAudioInputStreamTest, ReopensStreamSeveralTimes) {
  if (@available(macOS 14.2, *)) {
    int expected_create_aggregate_device_count = 0;
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    ++expected_create_aggregate_device_count;

    // An aggregated device is created once in Open()
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // Check if catap was recreated on default device change.
    fake_catap_api()->property_listener_block(
        1, &kDefaultOutputDevicePropertyAddress);
    ++expected_create_aggregate_device_count;
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    stream_->Start(&fake_callback_);

    // Check if catap was recreated on default device change after Start().
    fake_catap_api()->property_listener_block(
        1, &kDefaultOutputDevicePropertyAddress);
    ++expected_create_aggregate_device_count;
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // No change on sample rate notifications if sample rate haven't changed.
    fake_catap_api()->property_listener_block(1, &kSampleRateAddress);
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // Check if catap was recreated on real sampe rate change.
    fake_catap_api()->last_set_sample_rate = 16000;
    fake_catap_api()->property_listener_block(1, &kSampleRateAddress);
    ++expected_create_aggregate_device_count;
    EXPECT_EQ(fake_catap_api()->create_aggregate_device_count,
              expected_create_aggregate_device_count);

    // Check if the catap stream still calls the OnData() callback.
    EnsureStreamIsActive();
  }
}

TEST_F(CatapAudioInputStreamTest, CaptureStereoDeviceInStereo) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    CheckSuccessfulOpen();
    EXPECT_FALSE([fake_catap_api()->last_tap_description isMono]);

    stream_->Start(&fake_callback_);
    EnsureStreamIsActive();
    // Verify that the callback receives stereo audio.
    EXPECT_EQ(fake_callback_.last_number_of_channels(),
              kNumberOfChannelsStereo);
    EXPECT_EQ(fake_callback_.last_number_of_frames(),
              kCatapLoopbackDefaultFramesPerBuffer);
  }
}

TEST_F(CatapAudioInputStreamTest, ForceMonoCaptureForMonoDevice) {
  if (@available(macOS 14.2, *)) {
    CreateStream();
    fake_catap_api()->number_of_device_channels = kNumberOfChannelsMono;
    EXPECT_EQ(stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
    CheckSuccessfulOpen();
    EXPECT_TRUE([fake_catap_api()->last_tap_description isMono]);

    stream_->Start(&fake_callback_);
    EnsureStreamIsActive();
    // Verify that the callback receives stereo audio.
    EXPECT_EQ(fake_callback_.last_number_of_channels(),
              kNumberOfChannelsStereo);
    EXPECT_EQ(fake_callback_.last_number_of_frames(),
              kCatapLoopbackDefaultFramesPerBuffer);
  }
}

}  // namespace media
