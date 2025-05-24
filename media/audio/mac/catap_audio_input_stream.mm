// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/audio/mac/catap_audio_input_stream.h"

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CATapDescription.h>
#include <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#include <unistd.h>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/catap_api.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {
namespace {

// If this feature is enabled, the CoreAudio tap is probed after creation to
// verify that we have the proper permissions. If this fails the creation is
// reported as failed.
BASE_FEATURE(kMacCatapProbeTapOnCreation,
             "MacCatapProbeTapOnCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If this feature is enabled, we will only capture the default output device.
// If the feature is disabled, all system audio is captured regardless of which
// output device the audio is played on.
BASE_FEATURE(kMacCatapCaptureDefaultDevice,
             "MacCatapCaptureDefaultDevice",
             base::FEATURE_DISABLED_BY_DEFAULT);

API_AVAILABLE(macos(14.2))
OSStatus DeviceIoProc(AudioDeviceID,
                      const AudioTimeStamp*,
                      const AudioBufferList* input_data,
                      const AudioTimeStamp* input_time,
                      AudioBufferList* output_data,
                      const AudioTimeStamp* output_time,
                      void* client_data) {
  CatapAudioInputStream* catap_input_stream =
      (CatapAudioInputStream*)client_data;
  CHECK(catap_input_stream != nullptr);
  // SAFETY: The type of inputData cannot be changed since it's received from
  // the OS. Wrap it immediately using its specified size.
  base::span UNSAFE_BUFFERS(
      input_buffers(input_data->mBuffers, input_data->mNumberBuffers));

  catap_input_stream->OnCatapSample(input_buffers, input_time);

  return noErr;
}
}  // namespace

// 0.0 is used to indicate that this device doesn't support setting the volume.
// TODO(crbug.com/415953612): Is this okay, or do we need to support this?
constexpr float kMaxVolume = 0.0;

CatapAudioInputStream::CatapAudioInputStream(
    std::unique_ptr<CatapApi> catap_api,
    const AudioParameters& params,
    const std::string& device_id,
    const AudioManager::LogCallback log_callback,
    NotifyOnCloseCallback close_callback,
    const std::string& default_output_device_id)
    : catap_api_(std::move(catap_api)),
      params_(params),
      buffer_frames_duration_(
          AudioTimestampHelper::FramesToTime(params_.frames_per_buffer(),
                                             params_.sample_rate())),
      device_id_(device_id),
      audio_bus_(
          AudioBus::Create(params_.channels(), params_.frames_per_buffer())),
      sink_(nullptr),
      log_callback_(std::move(log_callback)),
      close_callback_(std::move(close_callback)),
      default_output_device_id_(default_output_device_id) {
  // TODO(crbug.com/415953671): Update this check to match the device IDs that
  // are supported.
  CHECK(AudioDeviceDescription::IsLoopbackDevice(device_id_));
  CHECK(!log_callback_.is_null());
  CHECK(catap_api_);

  // Only mono and stereo audio is supported.
  CHECK(params_.channels() == 1 || params_.channels() == 2);
}

CatapAudioInputStream::~CatapAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AudioInputStream::OpenOutcome CatapAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Open");
  // TODO(crbug.com/419323791): Add UMA logging of errors and duration of the
  // call to Open().

  if (is_device_open_) {
    SendLogMessage("%s => Device is already open.", __func__);
    return OpenOutcome::kFailed;
  }

  NSArray<NSNumber*>* process_audio_device_ids_to_exclude = @[];
  if (device_id_ == AudioDeviceDescription::kLoopbackWithoutChromeId) {
    // Get a list of all CoreAudio process device IDs that belong to the Chrome
    // audio service.
    pid_t chrome_audio_service_pid = getpid();
    process_audio_device_ids_to_exclude =
        GetProcessAudioDeviceIds(chrome_audio_service_pid);
    if (![process_audio_device_ids_to_exclude count]) {
      SendLogMessage("%s => Could not determine audio objects that belong to "
                     "the audio service.",
                     __func__);
    }
  }

  if (base::FeatureList::IsEnabled(kMacCatapCaptureDefaultDevice)) {
    // Mix all process audio streams destined for the selected device stream
    // except the given processes.
    tap_description_ = [[CATapDescription alloc]
        initExcludingProcesses:process_audio_device_ids_to_exclude
                  andDeviceUID:[NSString stringWithUTF8String:
                                             default_output_device_id_.c_str()]
                    withStream:0];
  } else {
    // Mix all processes to a stereo stream except the given processes.
    tap_description_ =
        [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:
                                      process_audio_device_ids_to_exclude];
  }

  if (params_.channels() == 1) {
    [tap_description_ setMono:YES];
  }
  [tap_description_ setName:@"ChromeAudioService"];
  [tap_description_ setPrivate:YES];

  // Initialization: Step 1.
  OSStatus status =
      catap_api_->AudioHardwareCreateProcessTap(tap_description_, &tap_);
  if (status != noErr) {
    SendLogMessage("%s => Error creating process tap.", __func__);
    return OpenOutcome::kFailed;
  }

  NSString* tap_uid = [[tap_description_ UUID] UUIDString];
  NSArray<NSDictionary*>* taps = @[
    @{
      @kAudioSubTapUIDKey : (NSString*)tap_uid,
      @kAudioSubTapDriftCompensationKey : @YES,
    },
  ];

  // Get a unique ID.
  NSUUID* uuid = [NSUUID UUID];
  NSString* unique_uid = [uuid UUIDString];

  NSDictionary* aggregate_device_properties_ = @{
    @kAudioAggregateDeviceNameKey : @"ChromeAudioAggregateDevice",
    @kAudioAggregateDeviceUIDKey : unique_uid,
    @kAudioAggregateDeviceTapListKey : taps,
    @kAudioAggregateDeviceTapAutoStartKey : @NO,
    @kAudioAggregateDeviceIsPrivateKey : @YES,
  };

  // Initialization: Step 2.
  // Create the aggregate device.
  status = catap_api_->AudioHardwareCreateAggregateDevice(
      (__bridge CFDictionaryRef)aggregate_device_properties_,
      &aggregate_device_id_);
  if (status != noErr) {
    SendLogMessage("%s => Error creating aggregate device.", __func__);
    return OpenOutcome::kFailed;
  }

  // Initialization: Step 3.
  // Attach callback to the aggregate device.
  status = catap_api_->AudioDeviceCreateIOProcID(
      aggregate_device_id_, DeviceIoProc, this, &tap_io_proc_id_);
  if (status != noErr) {
    SendLogMessage("%s => Error calling AudioDeviceCreateIOProcID.", __func__);
    return OpenOutcome::kFailed;
  }

  // Try to explicitly set a property, if this fails this is a sign that we
  // don't have audio capture permission.
  if (base::FeatureList::IsEnabled(kMacCatapProbeTapOnCreation) &&
      !ProbeAudioTapPermissions()) {
    SendLogMessage("%s => Error when probing audio tap permissions.", __func__);
    return OpenOutcome::kFailed;
  }

  // TODO(crbug.com/415953612): Verify that the tap's sample rate is the same as
  // the expected.
  is_device_open_ = true;
  return OpenOutcome::kSuccess;
}

void CatapAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Start");
  CHECK(callback);
  CHECK(is_device_open_);

  sink_ = callback;
  // Initialization: Step 4.
  // Start the aggregate device.
  OSStatus status =
      catap_api_->AudioDeviceStart(aggregate_device_id_, tap_io_proc_id_);
  if (status != noErr) {
    SendLogMessage("%s => Error starting the device.", __func__);
    sink_->OnError();
  }
}

void CatapAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Stop");
  if (!sink_) {
    return;
  }

  CHECK_NE(aggregate_device_id_, kAudioObjectUnknown);
  CHECK_NE(tap_io_proc_id_, nullptr);

  // Reversing Step 4.
  // The call to AudioDeviceStop is synchronous. It will not return until any
  // current callbacks have finished executing. The call to AudioDeviceStop()
  // succeeds even though AudioDeviceStart() has not been called.
  OSStatus status =
      catap_api_->AudioDeviceStop(aggregate_device_id_, tap_io_proc_id_);
  if (status != noErr) {
    SendLogMessage("%s => Error stopping the device.", __func__);
  }

  sink_ = nullptr;
}

void CatapAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Close");
  Stop();
  is_device_open_ = false;

  if (aggregate_device_id_ != kAudioObjectUnknown &&
      tap_io_proc_id_ != nullptr) {
    // Reversing Step 3.
    OSStatus status = catap_api_->AudioDeviceDestroyIOProcID(
        aggregate_device_id_, tap_io_proc_id_);
    if (status != noErr) {
      SendLogMessage("%s => Error destroying device IO process ID.", __func__);
    }
    tap_io_proc_id_ = nullptr;
  }

  if (aggregate_device_id_ != kAudioObjectUnknown) {
    // Reversing Step 2.
    OSStatus status =
        catap_api_->AudioHardwareDestroyAggregateDevice(aggregate_device_id_);
    if (status != noErr) {
      SendLogMessage("%s => Error destroying aggregate device.", __func__);
    }
    aggregate_device_id_ = kAudioObjectUnknown;
  }

  if (tap_ != kAudioObjectUnknown) {
    // Reversing Step 1.
    OSStatus status = catap_api_->AudioHardwareDestroyProcessTap(tap_);
    if (status != noErr) {
      SendLogMessage("%s => Error destroying process tap.", __func__);
    }
    tap_ = kAudioObjectUnknown;
  }

  if (tap_description_ != nil) {
    tap_description_ = nil;
  }

  // Notify the owner that the stream can be deleted.
  std::move(close_callback_).Run(this);
}

double CatapAudioInputStream::GetMaxVolume() {
  return kMaxVolume;
}

void CatapAudioInputStream::SetVolume(double volume) {
  // SetVolume() is not supported, ignore call.
}

double CatapAudioInputStream::GetVolume() {
  return kMaxVolume;
}

bool CatapAudioInputStream::IsMuted() {
  return false;
}

void CatapAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  return;
}

void CatapAudioInputStream::OnCatapSample(
    const base::span<const AudioBuffer> input_buffers,
    const AudioTimeStamp* input_time) {
  if (!(input_time->mFlags & kAudioTimeStampHostTimeValid)) {
    // TODO(crbug.com/417910390): Add workaround and log this event to see if it
    // happens.
    return;
  }

  base::TimeTicks capture_time =
      base::TimeTicks::FromMachAbsoluteTime(input_time->mHostTime);
  TRACE_EVENT1("audio", "CatapAudioInputStream::OnCatapSample", "capture_time",
               capture_time);

  for (auto buffer : input_buffers) {
    float* data = (float*)buffer.mData;
    int frames =
        buffer.mDataByteSize / (buffer.mNumberChannels * sizeof(Float32));
    CHECK_EQ(static_cast<unsigned int>(params_.channels()),
             buffer.mNumberChannels);
    CHECK_EQ(params_.frames_per_buffer(), frames);
    audio_bus_->FromInterleaved<Float32SampleTypeTraits>(data, frames);

    sink_->OnData(audio_bus_.get(), capture_time, kMaxVolume, {});

    capture_time += buffer_frames_duration_;
  }
}

NSArray<NSNumber*>* CatapAudioInputStream::GetProcessAudioDeviceIds(
    pid_t chrome_process_id) {
  // Returns all CoreAudio process audio device IDs that belong to the specified
  // process ID.

  // TODO(crbug.com/419323791): Add UMA logging of the duration of
  // GetProcessAudioDeviceIds().

  AudioObjectPropertyAddress property_address = {
      kAudioHardwarePropertyProcessObjectList, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 property_size;

  // Get all CoreAudio process audio device IDs (which are UInt32).
  OSStatus result = catap_api_->AudioObjectGetPropertyDataSize(
      kAudioObjectSystemObject, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &property_size);
  if (result != noErr) {
    SendLogMessage("%s => Could not get number of process audio device IDs.",
                   __func__);
    return @[];
  }

  UInt32 num_devices = property_size / sizeof(AudioDeviceID);
  auto device_ids = std::vector<AudioDeviceID>(num_devices);
  result = catap_api_->AudioObjectGetPropertyData(
      kAudioObjectSystemObject, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &property_size, device_ids.data());
  if (result != noErr) {
    SendLogMessage("%s => Could not get process audio device IDs.", __func__);
    return @[];
  }

  NSMutableArray<NSNumber*>* process_audio_device_ids_array =
      [NSMutableArray arrayWithCapacity:num_devices];

  for (AudioDeviceID device_id : device_ids) {
    // Get the process ID and add the device to the list if there's a match.
    property_address.mSelector = kAudioProcessPropertyPID;
    int32_t process_id;
    property_size = sizeof(int32_t);
    result = catap_api_->AudioObjectGetPropertyData(
        device_id, &property_address, /*in_qualifier_data_size=*/0,
        /*in_qualifier_data=*/nullptr, &property_size, &process_id);
    if (result != noErr) {
      SendLogMessage(
          "%s => Could not determine process ID of process audio device ID.",
          __func__);
      continue;  // Skip this device and continue to the next.
    }

    if (process_id == chrome_process_id) {
      [process_audio_device_ids_array addObject:@(device_id)];
    }
  }

  return process_audio_device_ids_array;
}

bool CatapAudioInputStream::ProbeAudioTapPermissions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CATapDescription* description;
  UInt32 propertySize = sizeof(CATapDescription*);
  AudioObjectPropertyAddress propertyAddress = {
      kAudioTapPropertyDescription, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  OSStatus status = catap_api_->AudioObjectGetPropertyData(
      tap_, &propertyAddress, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &propertySize, &description);

  if (status != noErr) {
    return false;
  }

  status = catap_api_->AudioObjectSetPropertyData(
      tap_, &propertyAddress, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, propertySize, &description);

  if (status != noErr) {
    return false;
  }
  return true;
}

void CatapAudioInputStream::SendLogMessage(const char* format, ...) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  va_list args;
  va_start(args, format);
  log_callback_.Run("CatapAudioInputStream::" +
                    base::StringPrintV(format, args));
  va_end(args);
}

AudioInputStream* CreateCatapAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    base::OnceCallback<void(AudioInputStream*)> close_callback,
    const std::string& default_output_device_id) {
  if (@available(macOS 14.2, *)) {
    return new CatapAudioInputStream(std::make_unique<CatapApiImpl>(), params,
                                     device_id, std::move(log_callback),
                                     std::move(close_callback),
                                     default_output_device_id);
  }
  return nullptr;
}

API_AVAILABLE(macos(14.2))
AudioInputStream* CreateCatapAudioInputStreamForTesting(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    base::OnceCallback<void(AudioInputStream*)> close_callback,
    const std::string& default_output_device_id,
    std::unique_ptr<CatapApi> catap_api) {
  return new CatapAudioInputStream(
      std::move(catap_api), params, device_id, std::move(log_callback),
      std::move(close_callback), default_output_device_id);
}

}  // namespace media
