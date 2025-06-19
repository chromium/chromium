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

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/catap_api.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {
namespace {
const char kCatapAudioInputStreamUmaBaseName[] =
    "Media.Audio.Mac.CatapAudioInputStream";
const char kHistogramPartsSeparator[] = ".";
const char kHistogramStatusPrefix[] = "Status";
const char kHistogramOperationDurationPrefix[] = "OperationDuration";
const char kHistogramOpenSuffix[] = "Open";
const char kHistogramStartSuffix[] = "Start";
const char kHistogramStopSuffix[] = "Stop";
const char kHistogramCloseSuffix[] = "Close";
const char kHistogramGetProcessAudioDeviceIdsSuffix[] =
    "GetProcessAudioDeviceIds";
const char kHistogramSuccessSuffix[] = "Success";
const char kHistogramFailureSuffix[] = "Failure";
const char kHostTimeStatusName[] = "HostTimeStatus";

// If this feature is enabled, the CoreAudio tap is probed after creation to
// verify that we have the proper permissions. If this fails the creation is
// reported as failed.
BASE_FEATURE(kMacCatapProbeTapOnCreation,
             "MacCatapProbeTapOnCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When `kMacCatapCaptureAllDevices` is disabled:
//
// CatapAudioInputStream captures audio from the default output device. However,
// if the device ID is explicitly set to `kLoopbackAllDevicesId`, it will
// capture all system audio regardless of the specific output device used for
// playback.
//
// When `kMacCatapCaptureAllDevices` is enabled:
//
// CatapAudioInputStream captures all system audio, irrespective of the specific
// output device it's played on or the device ID set.
BASE_FEATURE(kMacCatapCaptureAllDevices,
             "MacCatapCaptureAllDevices",
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

// Helper functions to generate histogram names.
std::string GetHistogramName(std::string_view status_prefix,
                             std::string_view operation_suffix,
                             std::string_view extra_suffix) {
  return base::JoinString({kCatapAudioInputStreamUmaBaseName, status_prefix,
                           operation_suffix, extra_suffix},
                          kHistogramPartsSeparator);
}

std::string GetHistogramName(std::string_view status_prefix,
                             std::string_view operation_suffix) {
  return base::JoinString(
      {kCatapAudioInputStreamUmaBaseName, status_prefix, operation_suffix},
      kHistogramPartsSeparator);
}

API_AVAILABLE(macos(14.2))
void ReportOpenStatus(CatapAudioInputStream::OpenStatus status,
                      base::TimeDelta duration) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kHistogramStatusPrefix, kHistogramOpenSuffix), status);
  base::UmaHistogramTimes(
      GetHistogramName(kHistogramOperationDurationPrefix, kHistogramOpenSuffix,
                       status == CatapAudioInputStream::OpenStatus::kOk
                           ? kHistogramSuccessSuffix
                           : kHistogramFailureSuffix),
      duration);
}

void ReportStartStatus(bool success, base::TimeDelta duration) {
  base::UmaHistogramBoolean(
      GetHistogramName(kHistogramStatusPrefix, kHistogramStartSuffix), success);
  base::UmaHistogramTimes(
      GetHistogramName(
          kHistogramOperationDurationPrefix, kHistogramStartSuffix,
          success ? kHistogramSuccessSuffix : kHistogramFailureSuffix),
      duration);
}

void ReportStopStatus(bool success, base::TimeDelta duration) {
  base::UmaHistogramBoolean(
      GetHistogramName(kHistogramStatusPrefix, kHistogramStopSuffix), success);
  base::UmaHistogramTimes(
      GetHistogramName(
          kHistogramOperationDurationPrefix, kHistogramStopSuffix,
          success ? kHistogramSuccessSuffix : kHistogramFailureSuffix),
      duration);
}

API_AVAILABLE(macos(14.2))
void ReportCloseStatus(CatapAudioInputStream::CloseStatus status,
                       base::TimeDelta duration) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kHistogramStatusPrefix, kHistogramCloseSuffix), status);
  base::UmaHistogramTimes(
      GetHistogramName(kHistogramOperationDurationPrefix, kHistogramCloseSuffix,
                       status == CatapAudioInputStream::CloseStatus::kOk
                           ? kHistogramSuccessSuffix
                           : kHistogramFailureSuffix),
      duration);
}

void ReportGetProcessAudioDeviceIdsDuration(bool success,
                                            base::TimeDelta duration) {
  base::UmaHistogramTimes(
      GetHistogramName(
          kHistogramOperationDurationPrefix,
          kHistogramGetProcessAudioDeviceIdsSuffix,
          success ? kHistogramSuccessSuffix : kHistogramFailureSuffix),
      duration);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HostTimeStatus {
  kNoMissingHostTime = 0,
  kSometimesMissingHostTimeNoRecover = 1,
  kSometimesMissingHostTimeRecovered = 2,
  kAlwaysMissingHostTime = 3,
  kMaxValue = kAlwaysMissingHostTime
};

HostTimeStatus GetHostTimeStatus(int total_callbacks,
                                 int callbacks_with_missing_host_time,
                                 bool has_recovered) {
  if (callbacks_with_missing_host_time == 0) {
    return HostTimeStatus::kNoMissingHostTime;
  }
  if (callbacks_with_missing_host_time == total_callbacks) {
    return HostTimeStatus::kAlwaysMissingHostTime;
  }

  return has_recovered ? HostTimeStatus::kSometimesMissingHostTimeRecovered
                       : HostTimeStatus::kSometimesMissingHostTimeNoRecover;
}

void ReportHostTimeStatus(int total_callbacks,
                          int callbacks_with_missing_host_time,
                          bool has_recovered) {
  base::UmaHistogramEnumeration(
      base::JoinString({kCatapAudioInputStreamUmaBaseName, kHostTimeStatusName},
                       kHistogramPartsSeparator),
      GetHostTimeStatus(total_callbacks, callbacks_with_missing_host_time,
                        has_recovered));
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
  CHECK(device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId ||
        device_id == AudioDeviceDescription::kLoopbackWithMuteDeviceId ||
        device_id == AudioDeviceDescription::kLoopbackWithoutChromeId ||
        device_id == AudioDeviceDescription::kLoopbackAllDevicesId);
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
  base::ElapsedTimer timer;

  if (is_device_open_) {
    ReportOpenStatus(OpenStatus::kErrorDeviceAlreadyOpen, timer.Elapsed());
    SendLogMessage("%s => Device is already open.", __func__);
    return OpenOutcome::kAlreadyOpen;
  }

  NSArray<NSNumber*>* process_audio_device_ids_to_exclude = @[];
  if (device_id_ == AudioDeviceDescription::kLoopbackWithoutChromeId) {
    // Get a list of all CoreAudio process device IDs that belong to the Chrome
    // audio service.
    pid_t chrome_audio_service_pid = getpid();
    process_audio_device_ids_to_exclude =
        GetProcessAudioDeviceIds(chrome_audio_service_pid);
    if (![process_audio_device_ids_to_exclude count]) {
      ReportOpenStatus(OpenStatus::kGetProcessAudioDeviceIdsReturnedEmpty,
                       timer.Elapsed());
      SendLogMessage("%s => Could not determine audio objects that belong to "
                     "the audio service.",
                     __func__);
    }
  }

  // The allocation and initialization of CATapDescription has been split into
  // the steps a-f to enable debugging of a flaky test.

  // a. Allocate the CATapDescription instance.
  //    Store it in a temporary variable first to allow immediate validation.
  CATapDescription* new_tap_description = [[CATapDescription alloc] init];

  // b. Check if allocation was successful.
  //    If alloc returns nil, it means memory allocation failed.
  if (new_tap_description == nil) {
    SendLogMessage("%s => Failed to allocate CATapDescription.", __func__);
    return OpenOutcome::kFailed;
  }

  // c. Verify the actual runtime class of the allocated object.
  //    This is the most critical check for an "unrecognized selector" when the
  //    API is known to exist. It catches cases where 'alloc' might return an
  //    object of an unexpected type due to subtle runtime issues.
  if (![new_tap_description isKindOfClass:[CATapDescription class]]) {
    SendLogMessage("%s => Allocated object is of unexpected class.", __func__);
    return OpenOutcome::kFailed;
  }

  // d. Double-check if the allocated object responds to the specific
  //    initializers. While logically redundant if step 3 passes and the OS
  //    version is correct, this directly tests the "unrecognized selector"
  //    condition.
  if (![new_tap_description respondsToSelector:@selector
                            (initStereoGlobalTapButExcludeProcesses:)]) {
    SendLogMessage("%s => CATapDescription instance does not respond to "
                   "initStereoGlobalTapButExcludeProcesses:.",
                   __func__);
    return OpenOutcome::kFailed;
  }
  if (![new_tap_description
          respondsToSelector:@selector(initExcludingProcesses:
                                                 andDeviceUID:withStream:)]) {
    SendLogMessage("%s => CATapDescription instance does not respond to "
                   "initExcludingProcesses:andDeviceUID:withStream:.",
                   __func__);
    return OpenOutcome::kFailed;
  }

  // e. Perform the actual initialization if all preceding checks pass.
  if (device_id_ == AudioDeviceDescription::kLoopbackAllDevicesId ||
      base::FeatureList::IsEnabled(kMacCatapCaptureAllDevices)) {
    // Mix all processes to a stereo stream except the given processes.
    tap_description_ =
        [new_tap_description initStereoGlobalTapButExcludeProcesses:
                                 process_audio_device_ids_to_exclude];
  } else {
    // Mix all process audio streams destined for the selected device stream
    // except the given processes.
    tap_description_ = [new_tap_description
        initExcludingProcesses:process_audio_device_ids_to_exclude
                  andDeviceUID:[NSString stringWithUTF8String:
                                             default_output_device_id_.c_str()]
                    withStream:0];
  }

  // f. Check if the initialization itself succeeded.
  //    An 'init' method can return nil if initialization fails internally for
  //    some reason.
  if (tap_description_ == nil) {
    SendLogMessage("%s => CATapDescription initialization failed.", __func__);
    return OpenOutcome::kFailed;
  }

  if (params_.channels() == 1) {
    [tap_description_ setMono:YES];
  }
  if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId) {
    // No audio is sent to the hardware (e.g, speakers) while the audio is
    // captured.
    [tap_description_ setMuteBehavior:CATapMuted];
  }
  [tap_description_ setName:@"ChromeAudioService"];
  [tap_description_ setPrivate:YES];

  // Initialization: Step 1.
  OSStatus status =
      catap_api_->AudioHardwareCreateProcessTap(tap_description_, &tap_);
  if (status != noErr || tap_ == kAudioObjectUnknown) {
    // `kAudioObjectUnknown` is returned if the specified output device doesn't
    // exist.
    ReportOpenStatus(OpenStatus::kErrorCreatingProcessTap, timer.Elapsed());
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
    ReportOpenStatus(OpenStatus::kErrorCreatingAggregateDevice,
                     timer.Elapsed());
    SendLogMessage("%s => Error creating aggregate device.", __func__);
    return OpenOutcome::kFailed;
  }

  // Configure the aggregate device.
  if (!ConfigureSampleRateOfAggregateDevice()) {
    ReportOpenStatus(OpenStatus::kErrorConfiguringSampleRate, timer.Elapsed());
    SendLogMessage(
        "%s => Could not configure the aggregate device with sample rate.",
        __func__);
    return OpenOutcome::kFailed;
  }
  if (!ConfigureFramesPerBufferOfAggregateDevice()) {
    ReportOpenStatus(OpenStatus::kErrorConfiguringFramesPerBuffer,
                     timer.Elapsed());
    SendLogMessage("%s => Could not configure the aggregate device with frame "
                   "buffer size.",
                   __func__);
    return OpenOutcome::kFailed;
  }

  // Initialization: Step 3.
  // Attach callback to the aggregate device.
  status = catap_api_->AudioDeviceCreateIOProcID(
      aggregate_device_id_, DeviceIoProc, this, &tap_io_proc_id_);
  if (status != noErr) {
    ReportOpenStatus(OpenStatus::kErrorCreatingIOProcID, timer.Elapsed());
    SendLogMessage("%s => Error calling AudioDeviceCreateIOProcID.", __func__);
    return OpenOutcome::kFailed;
  }

  // Try to explicitly set a property, if this fails this is a sign that we
  // don't have audio capture permission.
  if (base::FeatureList::IsEnabled(kMacCatapProbeTapOnCreation) &&
      !ProbeAudioTapPermissions()) {
    ReportOpenStatus(OpenStatus::kErrorMissingAudioTapPermission,
                     timer.Elapsed());
    SendLogMessage("%s => Error when probing audio tap permissions.", __func__);
    return OpenOutcome::kFailedSystemPermissions;
  }

  is_device_open_ = true;
  ReportOpenStatus(OpenStatus::kOk, timer.Elapsed());
  return OpenOutcome::kSuccess;
}

void CatapAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Start");
  base::ElapsedTimer timer;
  CHECK(callback);
  CHECK(is_device_open_);

  sink_ = callback;
  // Initialization: Step 4.
  // Start the aggregate device.
  OSStatus status =
      catap_api_->AudioDeviceStart(aggregate_device_id_, tap_io_proc_id_);
  if (status != noErr) {
    ReportStartStatus(false, timer.Elapsed());
    SendLogMessage("%s => Error starting the device.", __func__);
    sink_->OnError();
  }
  ReportStartStatus(true, timer.Elapsed());
}

void CatapAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Stop");
  base::ElapsedTimer timer;
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
    ReportStopStatus(false, timer.Elapsed());
    SendLogMessage("%s => Error stopping the device.", __func__);
  }

  ReportHostTimeStatus(total_callbacks_, callbacks_with_missing_host_time_,
                       recovered_from_missing_host_time_);

  sink_ = nullptr;
  ReportStopStatus(true, timer.Elapsed());
}

void CatapAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Close");
  base::ElapsedTimer timer;
  Stop();

  is_device_open_ = false;

  if (aggregate_device_id_ != kAudioObjectUnknown &&
      tap_io_proc_id_ != nullptr) {
    // Reversing Step 3.
    OSStatus status = catap_api_->AudioDeviceDestroyIOProcID(
        aggregate_device_id_, tap_io_proc_id_);
    if (status != noErr) {
      ReportCloseStatus(CloseStatus::kErrorDestroyingIOProcID, timer.Elapsed());
      SendLogMessage("%s => Error destroying device IO process ID.", __func__);
    }
    tap_io_proc_id_ = nullptr;
  }

  if (aggregate_device_id_ != kAudioObjectUnknown) {
    // Reversing Step 2.
    OSStatus status =
        catap_api_->AudioHardwareDestroyAggregateDevice(aggregate_device_id_);
    if (status != noErr) {
      ReportCloseStatus(CloseStatus::kErrorDestroyingAggregateDevice,
                        timer.Elapsed());
      SendLogMessage("%s => Error destroying aggregate device.", __func__);
    }
    aggregate_device_id_ = kAudioObjectUnknown;
  }

  if (tap_ != kAudioObjectUnknown) {
    // Reversing Step 1.
    OSStatus status = catap_api_->AudioHardwareDestroyProcessTap(tap_);
    if (status != noErr) {
      ReportCloseStatus(CloseStatus::kErrorDestroyingProcessTap,
                        timer.Elapsed());
      SendLogMessage("%s => Error destroying process tap.", __func__);
    }
    tap_ = kAudioObjectUnknown;
  }

  if (tap_description_ != nil) {
    tap_description_ = nil;
  }

  ReportCloseStatus(CloseStatus::kOk, timer.Elapsed());

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
  base::TimeTicks capture_time;
  if (!(input_time->mFlags & kAudioTimeStampHostTimeValid)) {
    // Fallback if there's no host time stamp. There's no evidence that this
    // ever happens, so this is just in case.
    capture_time = next_expected_capture_time_ ? *next_expected_capture_time_
                                               : base::TimeTicks::Now();
    ++callbacks_with_missing_host_time_;
  } else {
    capture_time = base::TimeTicks::FromMachAbsoluteTime(input_time->mHostTime);
    recovered_from_missing_host_time_ = callbacks_with_missing_host_time_ > 0;
  }
  ++total_callbacks_;
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

  // Store the current capture time and use as a fallback in case there's no
  // host time provided with the next callback.
  next_expected_capture_time_ = capture_time;
}

NSArray<NSNumber*>* CatapAudioInputStream::GetProcessAudioDeviceIds(
    pid_t chrome_process_id) {
  // Returns all CoreAudio process audio device IDs that belong to the specified
  // process ID.
  base::ElapsedTimer timer;

  AudioObjectPropertyAddress property_address = {
      kAudioHardwarePropertyProcessObjectList, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 property_size;

  // Get all CoreAudio process audio device IDs (which are UInt32).
  OSStatus result = catap_api_->AudioObjectGetPropertyDataSize(
      kAudioObjectSystemObject, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &property_size);
  if (result != noErr) {
    ReportGetProcessAudioDeviceIdsDuration(false, timer.Elapsed());
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
    ReportGetProcessAudioDeviceIdsDuration(false, timer.Elapsed());
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

  ReportGetProcessAudioDeviceIdsDuration(true, timer.Elapsed());
  return process_audio_device_ids_array;
}

bool CatapAudioInputStream::ConfigureSampleRateOfAggregateDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set sample rate.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 property_size = sizeof(Float64);
  Float64 sample_rate = params_.sample_rate();
  OSStatus result = catap_api_->AudioObjectSetPropertyData(
      aggregate_device_id_, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, property_size, &sample_rate);
  if (result != noErr) {
    SendLogMessage("%s => Could not set sample rate of the aggregate device.",
                   __func__);
    return false;
  }
  return true;
}

bool CatapAudioInputStream::ConfigureFramesPerBufferOfAggregateDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set frames per buffer.
  // Set sample rate.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 property_size = sizeof(UInt32);
  UInt32 frames_per_buffer = params_.frames_per_buffer();
  OSStatus result = catap_api_->AudioObjectSetPropertyData(
      aggregate_device_id_, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, property_size, &frames_per_buffer);
  if (result != noErr) {
    SendLogMessage(
        "%s => Could not set frames per buffer of the aggregate device.",
        __func__);
    return false;
  }
  return true;
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
