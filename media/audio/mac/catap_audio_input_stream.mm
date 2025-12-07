// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/catap_audio_input_stream.h"

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CATapDescription.h>
#include <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#include <MacTypes.h>
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
#include "media/audio/application_loopback_device_helper.h"
#include "media/audio/audio_features.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/catap_api.h"
#include "media/audio/mac/core_audio_util_mac.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {
namespace {
const char kCatapAudioInputStreamUmaBaseName[] =
    "Media.Audio.Mac.CatapAudioInputStream";

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
const char kHistogramDeviceIsAliveName[] = "IsAlive";
const char kHistogramChannelCountMismatchName[] = "ChannelCountMismatch";
const char kHistogramFramesMismatchName[] = "FramesMismatch";

// If this feature is enabled, the CoreAudio tap is probed after creation to
// verify that we have the proper permissions. If this fails the creation is
// reported as failed.
BASE_FEATURE(kMacCatapProbeTapOnCreation, base::FEATURE_ENABLED_BY_DEFAULT);

// When `kMacCatapCaptureAllDevices` is disabled:
//
// CatapAudioInputStreamSource captures audio from the default output device.
// However, if the device ID is explicitly set to `kLoopbackAllDevicesId`, it
// will capture all system audio regardless of the specific output device used
// for playback.
//
// When `kMacCatapCaptureAllDevices` is enabled:
//
// CatapAudioInputStreamSource captures all system audio, irrespective of the
// specific output device it's played on or the device ID set.
BASE_FEATURE(kMacCatapCaptureAllDevices, base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, mono capture is forced for mono devices. This
// will be upmixed to stereo in CatapAudioInputStreamSource if the output is
// configured to be sterero.
BASE_FEATURE(kMacCatapForceMonoCaptureOfMonoDevices,
             base::FEATURE_ENABLED_BY_DEFAULT);

API_AVAILABLE(macos(14.2))
OSStatus DeviceIoProc(AudioDeviceID,
                      const AudioTimeStamp*,
                      const AudioBufferList* input_data,
                      const AudioTimeStamp* input_time,
                      AudioBufferList* output_data,
                      const AudioTimeStamp* output_time,
                      void* client_data) {
  CatapAudioInputStreamSource* catap_input_stream =
      reinterpret_cast<CatapAudioInputStreamSource*>(client_data);
  CHECK(catap_input_stream != nullptr);

  // Multiple buffers correspond to multiple streams. This is not expected
  // during system audio capture, and the OnCatapSample() function is designed
  // to only process the first buffer. A DCHECK is used here to notify us in
  // debug builds if the OS provides more than one buffer. This would indicate
  // an unexpected change in behavior that requires investigation.
  DCHECK_EQ(input_data->mNumberBuffers, 1u);

  if (input_data->mNumberBuffers > 0 && input_data->mBuffers->mData != NULL) {
    catap_input_stream->OnCatapSample(input_data->mBuffers, input_time);
  }
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
void ReportOpenStatus(CatapAudioInputStreamSource::OpenStatus status,
                      base::TimeDelta duration) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kHistogramStatusPrefix, kHistogramOpenSuffix), status);
  base::UmaHistogramTimes(
      GetHistogramName(kHistogramOperationDurationPrefix, kHistogramOpenSuffix,
                       status == CatapAudioInputStreamSource::OpenStatus::kOk
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
void ReportCloseStatus(CatapAudioInputStreamSource::CloseStatus status,
                       base::TimeDelta duration) {
  base::UmaHistogramEnumeration(
      GetHistogramName(kHistogramStatusPrefix, kHistogramCloseSuffix), status);
  base::UmaHistogramTimes(
      GetHistogramName(kHistogramOperationDurationPrefix, kHistogramCloseSuffix,
                       status == CatapAudioInputStreamSource::CloseStatus::kOk
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

void ReportMismatchStatus(int total_callbacks_with_channel_count_mismatch,
                          int total_callbacks_with_frames_mismatch) {
  base::UmaHistogramCounts1000(
      base::JoinString({kCatapAudioInputStreamUmaBaseName,
                        kHistogramChannelCountMismatchName},
                       kHistogramPartsSeparator),
      total_callbacks_with_channel_count_mismatch);
  base::UmaHistogramCounts1000(
      base::JoinString(
          {kCatapAudioInputStreamUmaBaseName, kHistogramFramesMismatchName},
          kHistogramPartsSeparator),
      total_callbacks_with_frames_mismatch);
}

bool IsLoopbackDevice(const std::string& device_id) {
  return device_id == AudioDeviceDescription::kLoopbackInputDeviceId ||
         device_id == AudioDeviceDescription::kLoopbackWithMuteDeviceId ||
         device_id == AudioDeviceDescription::kLoopbackWithMuteDeviceIdCast ||
         device_id == AudioDeviceDescription::kLoopbackWithoutChromeId ||
         device_id == AudioDeviceDescription::kLoopbackAllDevicesId ||
         AudioDeviceDescription::IsApplicationLoopbackDevice(device_id);
}

// True if the capturer should be configured to only capture the default
// device.
bool IsDefaultOutputDeviceLoopback(const std::string& device_id) {
  return device_id != AudioDeviceDescription::kLoopbackAllDevicesId &&
         !AudioDeviceDescription::IsApplicationLoopbackDevice(device_id) &&
         !base::FeatureList::IsEnabled(kMacCatapCaptureAllDevices);
}

bool ExcludeChromeLoopback(const std::string& device_id) {
  return device_id == AudioDeviceDescription::kLoopbackWithoutChromeId;
}

bool MuteLocalPlaybackLoopback(const std::string& device_id) {
  return device_id == AudioDeviceDescription::kLoopbackWithMuteDeviceId ||
         device_id == AudioDeviceDescription::kLoopbackWithMuteDeviceIdCast;
}

// Returns AudioDeviceID and Unique ID (UID) for default output device, or
// `nullopt` if there were any errors.
API_AVAILABLE(macos(14.2))
CatapAudioInputStream::AudioDeviceIds GetDefaultOutputDeviceIds() {
  CatapAudioInputStream::AudioDeviceIds device_ids;
  device_ids.id = core_audio_mac::GetDefaultDevice(/*input=*/false);
  if (device_ids.id) {
    device_ids.uid = core_audio_mac::GetDeviceUniqueID(*device_ids.id);
  }
  return device_ids;
}

bool operator==(const AudioObjectPropertyAddress& x,
                const AudioObjectPropertyAddress& y) {
  return x.mSelector == y.mSelector && x.mScope == y.mScope &&
         x.mElement == y.mElement;
}

}  // namespace

// Helper class to manage CoreAudio property listeners.
//
// This class abstracts the process of adding and removing property listeners
// for CoreAudio objects. It listens for changes to the
// kAudioDevicePropertyDeviceIsAlive property of the aggregate device and,
// optionally, the kAudioHardwarePropertyDefaultOutputDevice property of the
// system object.
//
// The property listener block uses `dispatch_get_main_queue()` to ensure that
// property change notifications are delivered on the main thread. Using a weak
// pointer for the callback acts as a final safeguard to prevent a crash if a
// notification fires during the object's destruction.
class PropertyListenerHelper {
 public:
  using ProcessPropertyChangeCallback = base::RepeatingCallback<void(
      base::span<const AudioObjectPropertyAddress>)>;
  PropertyListenerHelper(
      bool capture_default_device,
      AudioObjectID aggregate_device_id,
      ProcessPropertyChangeCallback process_property_change_callback,
      const raw_ptr<CatapApi> catap_api)
      : capture_default_device_(capture_default_device),
        aggregate_device_id_(aggregate_device_id),
        catap_api_(catap_api) {
    AddPropertyListener(process_property_change_callback);
  }

  ~PropertyListenerHelper() { RemovePropertyListener(); }

 private:
  void AddPropertyListener(
      const ProcessPropertyChangeCallback process_property_change_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("audio", "PropertyListenerHelper::AddPropertyListener");
    property_listener_block_ = ^(UInt32 number_of_addresses,
                                 const AudioObjectPropertyAddress* addresses) {
      // SAFETY: The type of addresses cannot be changed since it's received
      // from the OS. Wrap it immediately using its specified size.
      base::span UNSAFE_BUFFERS(
          property_addresses(addresses, number_of_addresses));
      process_property_change_callback.Run(property_addresses);
    };

    catap_api_->AudioObjectAddPropertyListenerBlock(
        aggregate_device_id_, &kDeviceIsAliveAddress, dispatch_get_main_queue(),
        property_listener_block_);

    if (capture_default_device_) {
      catap_api_->AudioObjectAddPropertyListenerBlock(
          kAudioObjectSystemObject, &kDefaultOutputDevicePropertyAddress,
          dispatch_get_main_queue(), property_listener_block_);
    }

    catap_api_->AudioObjectAddPropertyListenerBlock(
        aggregate_device_id_, &kSampleRateAddress, dispatch_get_main_queue(),
        property_listener_block_);
  }

  void RemovePropertyListener() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("audio", "PropertyListenerHelper::RemovePropertyListener");

    // Use the stored block reference to remove the listener.
    catap_api_->AudioObjectRemovePropertyListenerBlock(
        aggregate_device_id_, &kSampleRateAddress, dispatch_get_main_queue(),
        property_listener_block_);

    if (capture_default_device_) {
      catap_api_->AudioObjectRemovePropertyListenerBlock(
          kAudioObjectSystemObject, &kDefaultOutputDevicePropertyAddress,
          dispatch_get_main_queue(), property_listener_block_);
    }

    catap_api_->AudioObjectRemovePropertyListenerBlock(
        aggregate_device_id_, &kDeviceIsAliveAddress, dispatch_get_main_queue(),
        property_listener_block_);

    property_listener_block_ = nil;
  }

  const bool capture_default_device_;

  const AudioObjectID aggregate_device_id_;

  // Interface used to access the CoreAudio framework.
  const raw_ptr<CatapApi> catap_api_;

  // A reference to the listener block is needed to remove the listener when the
  // capture stream is stopped.
  AudioObjectPropertyListenerBlock property_listener_block_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// 0.0 is used to indicate that this device doesn't support setting the volume.
// TODO(crbug.com/415953612): Is this okay, or do we need to support this?
constexpr float kMaxVolume = 0.0;

CatapAudioInputStreamSource::Config::Config(const AudioParameters& params,
                                            const std::string& device_id,
                                            bool force_mono_capture)
    : catap_channels(force_mono_capture ? 1 : params.channels()),
      output_channels(params.channels()),
      sample_rate(params.sample_rate()),
      frames_per_buffer(params.frames_per_buffer()),
      capture_default_device(IsDefaultOutputDeviceLoopback(device_id)),
      mute_local_device(MuteLocalPlaybackLoopback(device_id)),
      exclude_chrome(ExcludeChromeLoopback(device_id)),
      capture_application_process_id(
          AudioDeviceDescription::IsApplicationLoopbackDevice(device_id)
              ? std::make_optional(
                    GetApplicationIdFromApplicationLoopbackDeviceId(device_id))
              : std::nullopt) {}

std::string CatapAudioInputStreamSource::Config::AsHumanReadableString() const {
  std::ostringstream s;
  s << "output channels: " << output_channels
    << ", sample_rate: " << sample_rate
    << ", frames_per_buffer: " << frames_per_buffer
    << ", capture_default_device: " << capture_default_device
    << ", mute_local_device: " << mute_local_device
    << ", exclude_chrome: " << exclude_chrome
    << ", catap_channels: " << catap_channels;
  if (capture_application_process_id) {
    s << ", capture_application_process_id: "
      << *capture_application_process_id;
  }
  return s.str();
}

CatapAudioInputStreamSource::CatapAudioInputStreamSource(
    const raw_ptr<CatapApi> catap_api,
    const Config& config,
    const AudioManager::LogCallback log_callback,
    const raw_ptr<AudioPropertyChangeCallback> audio_property_change_callback)
    : catap_api_(catap_api),
      config_(config),
      buffer_frames_duration_(
          AudioTimestampHelper::FramesToTime(config_.frames_per_buffer,
                                             config_.sample_rate)),
      glitch_helper_(config_.sample_rate,
                     AudioGlitchInfo::Direction::kLoopback),
      audio_bus_(config_.catap_channels == 1
                     ? AudioBus::CreateWrapper(config_.output_channels)
                     : AudioBus::Create(config_.output_channels,
                                        config_.frames_per_buffer)),
      sink_(nullptr),
      log_callback_(std::move(log_callback)),
      audio_property_change_callback_(audio_property_change_callback) {
  CHECK(!log_callback_.is_null());
  CHECK(catap_api_);

  // Only mono and stereo audio is supported.
  CHECK(config_.output_channels == 1 || config_.output_channels == 2);
  CHECK(config_.catap_channels == 1 ||
        config_.catap_channels == config_.output_channels);

  SendLogMessage("%s({config=[%s]})", __func__,
                 config_.AsHumanReadableString().c_str());
}

CatapAudioInputStreamSource::~CatapAudioInputStreamSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Close();
  ReportAndResetStats();
}

AudioInputStream::OpenOutcome CatapAudioInputStreamSource::Open(
    std::optional<std::string> default_output_device_uid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStreamSource::Open");
  base::ElapsedTimer timer;

  SendLogMessage("%s", __func__);

  if (is_device_open_) {
    ReportOpenStatus(OpenStatus::kErrorDeviceAlreadyOpen, timer.Elapsed());
    SendLogMessage("%s => Device is already open.", __func__);
    return AudioInputStream::OpenOutcome::kAlreadyOpen;
  }

  if (config_.capture_application_process_id) {
    // Get a list of all CoreAudio process device IDs that belong to the
    // specified application process.
    pid_t application_pid = *config_.capture_application_process_id;
    NSArray<NSNumber*>* process_audio_device_ids_to_include =
        GetProcessAudioDeviceIds(application_pid);
    if (![process_audio_device_ids_to_include count]) {
      ReportOpenStatus(OpenStatus::kGetProcessAudioDeviceIdsReturnedEmpty,
                       timer.Elapsed());
      SendLogMessage("%s => Could not determine audio objects that belong to "
                     "the application process.",
                     __func__);
      return AudioInputStream::OpenOutcome::kFailed;
    }
    // Mix the given process to a stereo stream. We will not select default
    // device below when we capture application audio.
    tap_description_ = [[CATapDescription alloc]
        initStereoMixdownOfProcesses:process_audio_device_ids_to_include];
  } else {
    NSArray<NSNumber*>* process_audio_device_ids_to_exclude = @[];
    if (config_.exclude_chrome) {
      // Get a list of all CoreAudio process device IDs that belong to the
      // Chrome audio service.
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

    // Default initialization: Mix all processes to a stereo stream except the
    // given processes. The default output device is selected below unless the
    // device ID specifies that all devices should be captured.
    tap_description_ =
        [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:
                                      process_audio_device_ids_to_exclude];
  }

  if (tap_description_ == nil) {
    ReportOpenStatus(OpenStatus::kErrorCreatingTapDescription, timer.Elapsed());
    SendLogMessage("%s => CATapDescription initialization failed.", __func__);
    return AudioInputStream::OpenOutcome::kFailed;
  }

  if (config_.capture_default_device) {
    if (!default_output_device_uid) {
      ReportOpenStatus(OpenStatus::kGetDefaultDeviceUidEmpty, timer.Elapsed());
      SendLogMessage("%s => Error getting UID for default output device",
                     __func__);
      return AudioInputStream::OpenOutcome::kFailed;
    }
    // Select the default output device.
    tap_description_.deviceUID = @(default_output_device_uid->c_str());
    tap_description_.stream = @(0);
  }

  if (config_.catap_channels == 1) {
    [tap_description_ setMono:YES];
  }
  if (config_.mute_local_device) {
    // device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId ||
    //   device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceIdCast) {
    //  No audio is sent to the hardware (e.g, speakers) while the audio is
    //  captured.
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
    SendLogMessage("%s => Error creating process tap. Status: %d", __func__,
                   status);
    return AudioInputStream::OpenOutcome::kFailed;
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
    SendLogMessage("%s => Error creating aggregate device. Status: %d",
                   __func__, status);
    return AudioInputStream::OpenOutcome::kFailed;
  }

  // Configure the aggregate device.
  if (!ConfigureSampleRateOfAggregateDevice()) {
    ReportOpenStatus(OpenStatus::kErrorConfiguringSampleRate, timer.Elapsed());
    SendLogMessage(
        "%s => Could not configure the aggregate device with sample rate.",
        __func__);
    return AudioInputStream::OpenOutcome::kFailed;
  }
  if (!ConfigureFramesPerBufferOfAggregateDevice()) {
    ReportOpenStatus(OpenStatus::kErrorConfiguringFramesPerBuffer,
                     timer.Elapsed());
    SendLogMessage("%s => Could not configure the aggregate device with frame "
                   "buffer size.",
                   __func__);
    return AudioInputStream::OpenOutcome::kFailed;
  }

  // Initialization: Step 3.
  // Attach callback to the aggregate device.
  status = catap_api_->AudioDeviceCreateIOProcID(
      aggregate_device_id_, DeviceIoProc, this, &tap_io_proc_id_);
  if (status != noErr) {
    ReportOpenStatus(OpenStatus::kErrorCreatingIOProcID, timer.Elapsed());
    SendLogMessage("%s => Error calling AudioDeviceCreateIOProcID. Status: %d",
                   __func__, status);
    return AudioInputStream::OpenOutcome::kFailed;
  }

  // Try to explicitly set a property, if this fails this is a sign that we
  // don't have audio capture permission.
  if (base::FeatureList::IsEnabled(kMacCatapProbeTapOnCreation) &&
      !ProbeAudioTapPermissions()) {
    ReportOpenStatus(OpenStatus::kErrorMissingAudioTapPermission,
                     timer.Elapsed());
    SendLogMessage("%s => Error when probing audio tap permissions.", __func__);
    return AudioInputStream::OpenOutcome::kFailedSystemPermissions;
  }

  property_listener_ = std::make_unique<PropertyListenerHelper>(
      config_.capture_default_device, aggregate_device_id_,
      base::BindRepeating(&CatapAudioInputStreamSource::ProcessPropertyChange,
                          weak_ptr_factory_.GetWeakPtr()),
      catap_api_);

  is_device_open_ = true;
  ReportOpenStatus(OpenStatus::kOk, timer.Elapsed());
  return AudioInputStream::OpenOutcome::kSuccess;
}

void CatapAudioInputStreamSource::Start(
    AudioInputStream::AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStreamSource::Start");
  SendLogMessage("%s", __func__);
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
    SendLogMessage("%s => Error starting the device. Status: %d", __func__,
                   status);
    sink_->OnError();
  }
  ReportStartStatus(true, timer.Elapsed());
}

void CatapAudioInputStreamSource::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStreamSource::Stop");
  SendLogMessage("%s", __func__);
  base::ElapsedTimer timer;

  property_listener_.reset();

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
    SendLogMessage("%s => Error stopping the device. Status: %d", __func__,
                   status);
  }

  ReportHostTimeStatus(total_callbacks_, callbacks_with_missing_host_time_,
                       recovered_from_missing_host_time_);
  ReportMismatchStatus(total_callbacks_with_channel_count_mismatch_,
                       total_callbacks_with_frames_mismatch_);
  if (total_callbacks_with_channel_count_mismatch_ > 0) {
    SendLogMessage("%s => total_callbacks_with_channel_count_mismatch_: %d",
                   __func__, total_callbacks_with_channel_count_mismatch_);
  }
  if (total_callbacks_with_frames_mismatch_ > 0) {
    SendLogMessage("%s => total_callbacks_with_frames_mismatch_: %d", __func__,
                   total_callbacks_with_frames_mismatch_);
  }

  sink_ = nullptr;
  ReportStopStatus(true, timer.Elapsed());
  ReportAndResetStats();
}

void CatapAudioInputStreamSource::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStreamSource::Close");
  SendLogMessage("%s", __func__);
  // Check if stopped.
  CHECK(!sink_);
  base::ElapsedTimer timer;

  is_device_open_ = false;

  if (aggregate_device_id_ != kAudioObjectUnknown &&
      tap_io_proc_id_ != nullptr) {
    // Reversing Step 3.
    OSStatus status = catap_api_->AudioDeviceDestroyIOProcID(
        aggregate_device_id_, tap_io_proc_id_);
    if (status != noErr) {
      ReportCloseStatus(CloseStatus::kErrorDestroyingIOProcID, timer.Elapsed());
      SendLogMessage("%s => Error destroying device IO process ID. Status: %d",
                     __func__, status);
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
      SendLogMessage("%s => Error destroying aggregate device. Status: %d",
                     __func__, status);
    }
    aggregate_device_id_ = kAudioObjectUnknown;
  }

  if (tap_ != kAudioObjectUnknown) {
    // Reversing Step 1.
    OSStatus status = catap_api_->AudioHardwareDestroyProcessTap(tap_);
    if (status != noErr) {
      ReportCloseStatus(CloseStatus::kErrorDestroyingProcessTap,
                        timer.Elapsed());
      SendLogMessage("%s => Error destroying process tap. Status: %d", __func__,
                     status);
    }
    tap_ = kAudioObjectUnknown;
  }

  if (tap_description_ != nil) {
    tap_description_ = nil;
  }

  ReportCloseStatus(CloseStatus::kOk, timer.Elapsed());
}

void CatapAudioInputStreamSource::OnCatapSample(
    const AudioBuffer* input_buffer,
    const AudioTimeStamp* input_time) {
  CHECK(input_buffer);
  CHECK(input_time);
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
  TRACE_EVENT1("audio", "CatapAudioInputStreamSource::OnCatapSample",
               "capture_time", capture_time);

  float* data = (float*)input_buffer->mData;
  int frames = input_buffer->mDataByteSize /
               (input_buffer->mNumberChannels * sizeof(Float32));

  // The number of channels may change when a bluetooth device is captured and
  // the bluetooth profile is switched between A2DP and HFP. The sample rate
  // changes at the same time, this means that the property listener will detect
  // the change and call OnError(). We have not seen such case, but it could
  // happen that one buffer is received with the wrong number of channels.
  constexpr int kMaxNumberOfWarningReports = 10;
  if (static_cast<unsigned int>(config_.catap_channels) !=
      input_buffer->mNumberChannels) {
    ++total_callbacks_with_channel_count_mismatch_;
    if (total_callbacks_with_channel_count_mismatch_ <
        kMaxNumberOfWarningReports) {
      DLOG(WARNING)
          << "CatapAudioInputStream::OnCatapSample: Channel count mismatch, "
             "input_buffer->mNumberChannels: "
          << input_buffer->mNumberChannels
          << " config_.catap_channels: " << config_.catap_channels;
    }
    return;
  }
  if (frames != config_.frames_per_buffer) {
    ++total_callbacks_with_frames_mismatch_;
    if (total_callbacks_with_frames_mismatch_ < kMaxNumberOfWarningReports) {
      DLOG(WARNING) << "CatapAudioInputStream::OnCatapSample: "
                       "frames: "
                    << frames << " does not match config_.frames_per_buffer: "
                    << config_.frames_per_buffer;
    }
    return;
  }

  glitch_helper_.OnFramesReceived(*input_time, config_.frames_per_buffer);

  if (config_.catap_channels == 1) {
    // If the captured signal is mono, we may need to upmix it. This loop copies
    // the single mono channel to all output channels. For example, if
    // outputting to stereo, both left and right channels will get the same mono
    // data.

    // SAFETY: This comes from a struct provided by the OS and the number of
    // frames is calculated based on the information provided in the struct.
    base::span UNSAFE_BUFFERS(mono_data(data, (size_t)frames));
    audio_bus_->set_frames(frames);
    for (int i = 0; i < config_.output_channels; ++i) {
      audio_bus_->SetChannelData(i, mono_data);
    }
  } else {
    // The captured signal is already stereo, so we can de-interleave it
    // directly into the audio bus.
    audio_bus_->FromInterleaved<Float32SampleTypeTraits>(data, frames);
  }
  sink_->OnData(audio_bus_.get(), capture_time, kMaxVolume,
                glitch_helper_.ConsumeGlitchInfo());

  // Stores the time of the next expected audio callback. This is used as a
  // fallback if the host doesn't provide a timestamp.
  next_expected_capture_time_ = capture_time + buffer_frames_duration_;
}

NSArray<NSNumber*>* CatapAudioInputStreamSource::GetProcessAudioDeviceIds(
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
    SendLogMessage(
        "%s => Could not get number of process audio device IDs. Status: %d",
        __func__, result);
    return @[];
  }

  UInt32 num_devices = property_size / sizeof(AudioDeviceID);
  auto device_ids = std::vector<AudioDeviceID>(num_devices);
  result = catap_api_->AudioObjectGetPropertyData(
      kAudioObjectSystemObject, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &property_size, device_ids.data());
  if (result != noErr) {
    ReportGetProcessAudioDeviceIdsDuration(false, timer.Elapsed());
    SendLogMessage("%s => Could not get process audio device IDs. Status: %d",
                   __func__, result);
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
      SendLogMessage("%s => Could not determine process ID of process audio "
                     "device ID. Status: %d",
                     __func__, result);
      continue;  // Skip this device and continue to the next.
    }

    if (process_id == chrome_process_id) {
      [process_audio_device_ids_array addObject:@(device_id)];
    }
  }

  ReportGetProcessAudioDeviceIdsDuration(true, timer.Elapsed());
  return process_audio_device_ids_array;
}

bool CatapAudioInputStreamSource::ConfigureSampleRateOfAggregateDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set sample rate.
  UInt32 property_size = sizeof(Float64);
  Float64 sample_rate = config_.sample_rate;
  OSStatus result = catap_api_->AudioObjectSetPropertyData(
      aggregate_device_id_, &kSampleRateAddress, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, property_size, &sample_rate);
  if (result != noErr) {
    SendLogMessage(
        "%s => Could not set sample rate of the aggregate device. Status: %d",
        __func__, result);
    return false;
  }
  return true;
}

std::optional<double>
CatapAudioInputStreamSource::GetSampleRateOfAggregateDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get sample rate.
  UInt32 property_size = sizeof(Float64);
  Float64 sample_rate = 0.0;
  OSStatus result = catap_api_->AudioObjectGetPropertyData(
      aggregate_device_id_, &kSampleRateAddress, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &property_size, &sample_rate);
  if (result != noErr) {
    SendLogMessage(
        "%s => Could not get sample rate of the aggregate device. Status: %d",
        __func__, result);
    return std::nullopt;
  }
  return sample_rate;
}

bool CatapAudioInputStreamSource::ConfigureFramesPerBufferOfAggregateDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set frames per buffer.
  // Set sample rate.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  UInt32 property_size = sizeof(UInt32);
  UInt32 frames_per_buffer = config_.frames_per_buffer;
  OSStatus result = catap_api_->AudioObjectSetPropertyData(
      aggregate_device_id_, &property_address, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, property_size, &frames_per_buffer);
  if (result != noErr) {
    SendLogMessage("%s => Could not set frames per buffer of the aggregate "
                   "device. Status: %d",
                   __func__, result);
    return false;
  }
  return true;
}

bool CatapAudioInputStreamSource::ProbeAudioTapPermissions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UInt32 propertySize = sizeof(CATapDescription*);
  AudioObjectPropertyAddress propertyAddress = {
      kAudioTapPropertyDescription, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};

  void* get_description_ptr = nullptr;
  OSStatus status = catap_api_->AudioObjectGetPropertyData(
      tap_, &propertyAddress, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, &propertySize, &get_description_ptr);
  if (status != noErr) {
    return false;
  }

  // We receive ownership of the Core Foundation object returned by
  // `AudioObjectGetPropertyData()`. `CFBridgingRelease` transfers this
  // ownership to ARC. The `description` object will now be released
  // automatically when it goes out of scope.
  CATapDescription* description = CFBridgingRelease(get_description_ptr);

  // `AudioObjectSetPropertyData()` does not take ownership of the object. We
  // use a non-owning `__bridge` cast to pass the pointer. ARC retains
  // ownership, and `description` will be released when it goes out of scope.
  void* set_description_ptr = (__bridge void*)description;
  status = catap_api_->AudioObjectSetPropertyData(
      tap_, &propertyAddress, /*in_qualifier_data_size=*/0,
      /*in_qualifier_data=*/nullptr, propertySize, &set_description_ptr);

  if (status != noErr) {
    return false;
  }
  return true;
}

void CatapAudioInputStreamSource::ProcessPropertyChange(
    base::span<const AudioObjectPropertyAddress> property_addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const AudioObjectPropertyAddress& property_address :
       property_addresses) {
    if (property_address == kDeviceIsAliveAddress) {
      TRACE_EVENT1("audio",
                   "CatapAudioInputStreamSource::ProcessPropertyChange",
                   "property", "DeviceIsAlive");
      // Read IsAlive property.
      UInt32 property_size = sizeof(UInt32);
      UInt32 is_alive = false;
      OSStatus status = catap_api_->AudioObjectGetPropertyData(
          aggregate_device_id_, &kDeviceIsAliveAddress,
          /*in_qualifier_data_size=*/0,
          /*in_qualifier_data=*/nullptr, &property_size, &is_alive);
      if (status != noErr) {
        continue;
      }
      base::UmaHistogramBoolean(
          base::JoinString(
              {kCatapAudioInputStreamUmaBaseName, kHistogramDeviceIsAliveName},
              kHistogramPartsSeparator),
          is_alive);
      SendLogMessage("%s => Device is alive property changed: %d", __func__,
                     is_alive);
      if (!is_alive) {
        OnError();
      }
    } else if (property_address == kDefaultOutputDevicePropertyAddress) {
      TRACE_EVENT1("audio",
                   "CatapAudioInputStreamSource::ProcessPropertyChange",
                   "property", "DefaultOutputDevice");
      SendLogMessage("%s => Default output device changed.", __func__);
      // Nothing should be done after the callback is called, because 'this'
      // might be deleted within the callback implementation.
      audio_property_change_callback_->OnDefaultDeviceChange();
    } else if (property_address == kSampleRateAddress) {
      TRACE_EVENT1("audio",
                   "CatapAudioInputStreamSource::ProcessPropertyChange",
                   "property", "SampleRate");
      std::optional<double> sample_rate = GetSampleRateOfAggregateDevice();
      if (!sample_rate.has_value() ||
          sample_rate.value() != config_.sample_rate) {
        SendLogMessage("%s => Sample rate changed. New sample rate: %f",
                       __func__, sample_rate.value_or(-1.0));
        // Nothing should be done after the callback is called, because 'this'
        // might be deleted within the callback implementation.
        audio_property_change_callback_->OnSampleRateChange();
      }
    }
  }
}

void CatapAudioInputStreamSource::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendLogMessage("%s", __func__);
  if (sink_) {
    sink_->OnError();
  }
}

void CatapAudioInputStreamSource::SendLogMessage(const char* format, ...) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  va_list args;
  va_start(args, format);
  log_callback_.Run("CatapAudioInputStreamSource::" +
                    base::StringPrintV(format, args));
  va_end(args);
}

void CatapAudioInputStreamSource::ReportAndResetStats() {
  std::optional<std::string> log_message =
      glitch_helper_.LogAndReset("CATap in");
  if (log_message) {
    SendLogMessage(log_message->c_str());
  }
}

CatapAudioInputStream::AudioDeviceIds::AudioDeviceIds() = default;
CatapAudioInputStream::AudioDeviceIds::~AudioDeviceIds() = default;
CatapAudioInputStream::AudioDeviceIds::AudioDeviceIds(
    const AudioDeviceIds& other) = default;
CatapAudioInputStream::AudioDeviceIds::AudioDeviceIds(AudioDeviceID device_id,
                                                      std::string uid)
    : id(device_id), uid(std::move(uid)) {}

CatapAudioInputStream::CatapAudioInputStream(
    std::unique_ptr<CatapApi> catap_api,
    GetDefaultDeviceIdsCallback get_default_device_ids_callback,
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    NotifyOnCloseCallback close_callback)
    : catap_api_(std::move(catap_api)),
      params_(params),
      device_id_(device_id),
      restart_on_device_change_(IsDefaultOutputDeviceLoopback(device_id_) &&
                                base::FeatureList::IsEnabled(
                                    features::kMacCatapRestartOnDeviceChange)),
      close_callback_(std::move(close_callback)),
      log_callback_(log_callback),

      get_default_device_ids_callback_(
          std::move(get_default_device_ids_callback)) {
  CHECK(IsLoopbackDevice(device_id_));
}

AudioInputStream::OpenOutcome CatapAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Open");

  if (source_) {
    return AudioInputStream::OpenOutcome::kAlreadyOpen;
  }

  AudioDeviceIds default_device_ids = get_default_device_ids_callback_.Run();

  // The microphone input from Bluetooth headsets using the headset profile
  // is mono. Typically the OS handles this and provides a stereo stream,
  // but this mechanism can fail. Forcing a direct mono capture for these
  // mono sources serves as a workaround.
  bool force_mono_capture =
      IsDefaultOutputDeviceLoopback(device_id_) &&
      base::FeatureList::IsEnabled(kMacCatapForceMonoCaptureOfMonoDevices) &&
      GetVirtualFormatChannels(
          default_device_ids.id.value_or(kAudioObjectUnknown)) == 1;

  CatapAudioInputStreamSource::Config config(params_, device_id_,
                                             force_mono_capture);

  if (AudioDeviceDescription::IsApplicationLoopbackDevice(device_id_) &&
      !config.capture_application_process_id) {
    SendLogMessage("%s => No valid Application PID to capture.", __func__);
    return AudioInputStream::OpenOutcome::kFailed;
  }

  source_ = std::make_unique<CatapAudioInputStreamSource>(
      catap_api_.get(), config, log_callback_, this);

  AudioInputStream::OpenOutcome outcome = source_->Open(default_device_ids.uid);

  if (outcome != OpenOutcome::kSuccess) {
    SendLogMessage("%s => Failed to open(), outcome: %d", __func__, outcome);
    source_.reset();
  }

  return outcome;
}

void CatapAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Start");
  CHECK(callback);
  if (!source_) {
    SendLogMessage("%s => stream is nullptr", __func__);
    callback->OnError();
    return;
  }
  audio_input_callback_ = callback;
  source_->Start(audio_input_callback_);
}

void CatapAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Stop");
  if (!audio_input_callback_) {
    return;
  }
  CHECK(source_);
  source_->Stop();
  audio_input_callback_ = nullptr;
}

void CatapAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::Close");
  Stop();
  source_.reset();

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

void CatapAudioInputStream::OnSampleRateChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (restart_on_device_change_) {
    RestartStream();
  } else {
    OnError();
  }
}

void CatapAudioInputStream::OnDefaultDeviceChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (restart_on_device_change_) {
    RestartStream();
  }
}

CatapAudioInputStream::~CatapAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!source_);
}

void CatapAudioInputStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendLogMessage("%s", __func__);
  if (audio_input_callback_) {
    audio_input_callback_->OnError();
  }
}

int CatapAudioInputStream::GetVirtualFormatChannels(AudioDeviceID device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamBasicDescription stream_format;
  UInt32 property_size = sizeof(AudioStreamBasicDescription);
  // Get the Virtual Format data.
  OSStatus status = catap_api_->AudioObjectGetPropertyData(
      device_id, &kVirtualFormatAddress, 0, NULL, &property_size,
      &stream_format);

  if (status != noErr) {
    return 0;
  }

  return stream_format.mChannelsPerFrame;
}

void CatapAudioInputStream::RestartStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "CatapAudioInputStream::RestartStream");
  // There can't be any audio property callbacks (OnSampleRateChange() and
  // OnDefaultDeviceChange()) from the `source_` if it doesn't exist. And
  // RestartStream() is only called from the property callbacks. Therefore
  // `source_` will always exist.
  CHECK(source_);
  source_->Stop();
  source_.reset();
  if (Open() != OpenOutcome::kSuccess) {
    CHECK(!source_);
    OnError();
    audio_input_callback_ = nullptr;
    return;
  }
  if (audio_input_callback_) {
    // The existence of an audio callback implies the previous `source_` was
    // active. Start the new `source_` immediately to maintain the stream's
    // started state.
    source_->Start(audio_input_callback_);
  }
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
    base::OnceCallback<void(AudioInputStream*)> close_callback) {
  if (@available(macOS 14.2, *)) {
    return new CatapAudioInputStream(
        std::make_unique<CatapApiImpl>(),
        base::BindRepeating(GetDefaultOutputDeviceIds), params, device_id,
        log_callback, std::move(close_callback));
  }
  log_callback.Run("CatapAudioInputStream::CreateCatapAudioInputStream() Catap "
                   "not supported");
  return nullptr;
}

API_AVAILABLE(macos(14.2))
AudioInputStream* CreateCatapAudioInputStreamForTesting(
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback,
    base::OnceCallback<void(AudioInputStream*)> close_callback,
    std::unique_ptr<CatapApi> catap_api,
    base::RepeatingCallback<CatapAudioInputStream::AudioDeviceIds()>
        get_default_device_ids_callback) {
  return new CatapAudioInputStream(
      std::move(catap_api), std::move(get_default_device_ids_callback), params,
      device_id, log_callback, std::move(close_callback));
}

}  // namespace media
