// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/mac/audio_manager_mac.h"

#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/memory/free_deleter.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/audio/apple/audio_auhal.h"
#include "media/audio/apple/audio_input.h"
#include "media/audio/apple/audio_low_latency_input.h"
#include "media/audio/apple/scoped_audio_unit.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/core_audio_util_mac.h"
#include "media/audio/mac/screen_capture_kit_swizzler.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/mac/audio_latency_mac.h"
#include "media/base/media_switches.h"

namespace media {

BASE_FEATURE(kMonitorOutputSampleRateChangesMac,
             "MonitorOutputSampleRateChangesMac",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample-rate on most Apple hardware.
static const int kFallbackSampleRate = 44100;

static bool GetOutputDeviceChannelsAndLayout(AudioUnit audio_unit,
                                             int* channels,
                                             ChannelLayout* channel_layout);

// Helper method to construct AudioObjectPropertyAddress structure given
// property selector and scope. The property element is always set to
// kAudioObjectPropertyElementMain.
static AudioObjectPropertyAddress GetAudioObjectPropertyAddress(
    AudioObjectPropertySelector selector,
    bool is_input) {
  AudioObjectPropertyScope scope = is_input ? kAudioObjectPropertyScopeInput
                                            : kAudioObjectPropertyScopeOutput;
  AudioObjectPropertyAddress property_address = {
      selector, scope, kAudioObjectPropertyElementMain};
  return property_address;
}

static const AudioObjectPropertyAddress kNoiseReductionPropertyAddress = {
    'nzca', kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMain};

// Get IO buffer size range from HAL given device id and scope.
static OSStatus GetIOBufferFrameSizeRange(AudioDeviceID device_id,
                                          bool is_input,
                                          UInt32* minimum,
                                          UInt32* maximum) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  AudioObjectPropertyAddress address = GetAudioObjectPropertyAddress(
      kAudioDevicePropertyBufferFrameSizeRange, is_input);
  AudioValueRange range = {0, 0};
  UInt32 data_size = sizeof(AudioValueRange);
  OSStatus result = AudioObjectGetPropertyData(device_id, &address, 0, NULL,
                                               &data_size, &range);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Failed to query IO buffer size range for device: " << std::hex
        << device_id;
  } else {
    *minimum = range.mMinimum;
    *maximum = range.mMaximum;
  }
  return result;
}

static bool HasAudioHardware(AudioObjectPropertySelector selector) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceID output_device_id = kAudioObjectUnknown;
  const AudioObjectPropertyAddress property_address = {
      selector,
      kAudioObjectPropertyScopeGlobal,  // mScope
      kAudioObjectPropertyElementMain   // mElement
  };
  UInt32 output_device_id_size = static_cast<UInt32>(sizeof(output_device_id));
  OSStatus err =
      AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address,
                                 0,     // inQualifierDataSize
                                 NULL,  // inQualifierData
                                 &output_device_id_size, &output_device_id);
  return err == kAudioHardwareNoError &&
         output_device_id != kAudioObjectUnknown;
}

static std::string GetAudioDeviceNameFromDeviceId(AudioDeviceID device_id,
                                                  bool is_input) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CFStringRef device_name = nullptr;
  UInt32 data_size = sizeof(device_name);
  AudioObjectPropertyAddress property_address = GetAudioObjectPropertyAddress(
      kAudioDevicePropertyDeviceNameCFString, is_input);
  OSStatus result = AudioObjectGetPropertyData(
      device_id, &property_address, 0, nullptr, &data_size, &device_name);
  std::string device;
  if (result == noErr) {
    device = base::SysCFStringRefToUTF8(device_name);
    CFRelease(device_name);
  }
  return device;
}

// Retrieves information on audio devices, and prepends the default
// device to the list if the list is non-empty.
static void GetAudioDeviceInfo(bool is_input,
                               media::AudioDeviceNames* device_names) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  std::vector<AudioObjectID> device_ids =
      core_audio_mac::GetAllAudioDeviceIDs();
  for (AudioObjectID device_id : device_ids) {
    const bool is_valid_for_direction =
        (is_input ? core_audio_mac::IsInputDevice(device_id)
                  : core_audio_mac::IsOutputDevice(device_id));

    if (!is_valid_for_direction) {
      continue;
    }

    std::optional<std::string> unique_id =
        core_audio_mac::GetDeviceUniqueID(device_id);
    if (!unique_id) {
      continue;
    }

    std::optional<std::string> label =
        core_audio_mac::GetDeviceLabel(device_id, is_input);
    if (!label) {
      continue;
    }

    // Filter out aggregate devices, e.g. those that get created by using
    // kAudioUnitSubType_VoiceProcessingIO.
    if (core_audio_mac::IsPrivateAggregateDevice(device_id)) {
      continue;
    }

    device_names->emplace_back(std::move(*label), std::move(*unique_id));
  }

  if (!device_names->empty()) {
    // Prepend the default device to the list since we always want it to be
    // on the top of the list for all platforms. There is no duplicate
    // counting here since the default device has been abstracted out before.
    device_names->push_front(media::AudioDeviceName::CreateDefault());
  }
}

AudioDeviceID AudioManagerMac::GetAudioDeviceIdByUId(
    bool is_input,
    const std::string& device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  AudioObjectPropertyAddress property_address = {
      kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  AudioDeviceID audio_device_id = kAudioObjectUnknown;
  UInt32 device_size = sizeof(audio_device_id);
  OSStatus result = -1;

  if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
    // Default Device.
    property_address.mSelector =
        is_input ? kAudioHardwarePropertyDefaultInputDevice
                 : kAudioHardwarePropertyDefaultOutputDevice;

    result =
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address,
                                   0, 0, &device_size, &audio_device_id);
  } else {
    // Non-default device.
    base::apple::ScopedCFTypeRef<CFStringRef> uid(
        base::SysUTF8ToCFStringRef(device_id));
    AudioValueTranslation value;
    value.mInputData = &uid;
    value.mInputDataSize = sizeof(CFStringRef);
    value.mOutputData = &audio_device_id;
    value.mOutputDataSize = device_size;
    UInt32 translation_size = sizeof(AudioValueTranslation);

    property_address.mSelector = kAudioHardwarePropertyDeviceForUID;
    result =
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address,
                                   0, 0, &translation_size, &value);
  }

  if (result) {
    OSSTATUS_DLOG(WARNING, result)
        << "Unable to query device " << device_id << " for AudioDeviceID";
  }

  return audio_device_id;
}

static bool GetDefaultDevice(AudioDeviceID* device, bool input) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(device);

  // Obtain the AudioDeviceID of the default input or output AudioDevice.
  AudioObjectPropertyAddress pa;
  pa.mSelector = input ? kAudioHardwarePropertyDefaultInputDevice
                       : kAudioHardwarePropertyDefaultOutputDevice;
  pa.mScope = kAudioObjectPropertyScopeGlobal;
  pa.mElement = kAudioObjectPropertyElementMain;

  UInt32 size = sizeof(*device);
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &pa, 0,
                                               0, &size, device);
  if ((result != kAudioHardwareNoError) || (*device == kAudioDeviceUnknown)) {
    DLOG(ERROR) << "Error getting default AudioDevice.";
    return false;
  }
  return true;
}

bool AudioManagerMac::GetDefaultInputDevice(AudioDeviceID* input_device) {
  return GetDefaultDevice(input_device, true);
}

bool AudioManagerMac::GetDefaultOutputDevice(AudioDeviceID* output_device) {
  return GetDefaultDevice(output_device, false);
}

// Returns the total number of channels on a device; regardless of what the
// device's preferred rendering layout looks like. Should only be used for the
// channel count when a device has more than kMaxConcurrentChannels.
static bool GetDeviceTotalChannelCount(AudioDeviceID device,
                                       AudioObjectPropertyScope scope,
                                       int* channels) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(channels);

  // Get the stream configuration of the device in an AudioBufferList (with the
  // buffer pointers set to nullptr) which describes the list of streams and the
  // number of channels in each stream.
  AudioObjectPropertyAddress pa = {kAudioDevicePropertyStreamConfiguration,
                                   scope, kAudioObjectPropertyElementMain};

  UInt32 size;
  OSStatus result = AudioObjectGetPropertyDataSize(device, &pa, 0, 0, &size);
  if (result != noErr || !size) {
    return false;
  }

  auto list_storage = base::HeapArray<uint8_t>::Uninit(size);
  AudioBufferList* buffer_list =
      reinterpret_cast<AudioBufferList*>(list_storage.data());

  result = AudioObjectGetPropertyData(device, &pa, 0, 0, &size, buffer_list);
  if (result != noErr) {
    return false;
  }

  // Determine number of channels based on the AudioBufferList.
  // |mNumberBuffers] is the  number of interleaved channels in the buffer.
  // If the number is 1, the buffer is noninterleaved.
  *channels = 0;
  for (UInt32 i = 0; i < buffer_list->mNumberBuffers; ++i) {
    *channels += buffer_list->mBuffers[i].mNumberChannels;
  }

  DVLOG(1) << __FUNCTION__
           << (scope == kAudioDevicePropertyScopeInput ? " Input" : " Output")
           << " total channels: " << *channels;
  return true;
}

// Returns the channel count from the |audio_unit|'s stream format for input
// scope / input element or output scope / output element.
static bool GetAudioUnitStreamFormatChannelCount(AudioUnit audio_unit,
                                                 AUElement element,
                                                 int* channels) {
  AudioStreamBasicDescription stream_format;
  UInt32 size = sizeof(stream_format);
  OSStatus result =
      AudioUnitGetProperty(audio_unit, kAudioUnitProperty_StreamFormat,
                           element == AUElement::OUTPUT ? kAudioUnitScope_Output
                                                        : kAudioUnitScope_Input,
                           element, &stream_format, &size);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "Failed to get AudioUnit stream format.";
    return false;
  }

  *channels = stream_format.mChannelsPerFrame;
  return true;
}

// Returns the `channels` for `device` as provided by the AudioUnit attached
// to that input device. Returns true if the `channels` could be pulled
// from the AudioUnit successfully, otherwise return false and `channels` is
// untouched.
static bool GetInputDeviceChannels(AudioDeviceID device, int* channels) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(channels);

  // For input, get the channel count directly from the AudioUnit's stream
  // format.
  // TODO(crbug.com/41361558): Find out if we can use channel layout on
  // input element, or confirm that we can't.
  ScopedAudioUnit au(device, AUElement::INPUT);
  if (!au.is_valid()) {
    return false;
  }

  if (!GetAudioUnitStreamFormatChannelCount(au.audio_unit(), AUElement::INPUT,
                                            channels)) {
    return false;
  }

  DVLOG(2) << __FUNCTION__ << " Input channels: " << *channels;
  return true;
}

// Returns the `channels` and `channel_layout` for `device` as provided by the
// AudioUnit attached to that output device. Returns true if the `channels` and
// `channel_layout` could be pulled from the AudioUnit successfully, otherwise
// return false and `channels` and `channel_layout` are untouched.
static bool GetOutputDeviceChannelsAndLayout(AudioDeviceID device,
                                             int* channels,
                                             ChannelLayout* channel_layout) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(channels);
  CHECK(channel_layout);

  // If the device has more channels than possible for layouts to express, use
  // the total count of channels on the device; as of this writing, macOS will
  // only return up to 8 channels in any layout. To allow WebAudio to work with
  // > 8 channel devices, we must use the total channel count instead of the
  // channel count of the preferred layout.
  int total_channel_count = 0;
  if (GetDeviceTotalChannelCount(device, kAudioDevicePropertyScopeOutput,
                                 &total_channel_count) &&
      total_channel_count > kMaxConcurrentChannels) {
    *channels = total_channel_count;
    *channel_layout = CHANNEL_LAYOUT_DISCRETE;
  } else {
    ScopedAudioUnit au(device, AUElement::OUTPUT);
    if (!au.is_valid()) {
      return false;
    }

    if (!GetOutputDeviceChannelsAndLayout(au.audio_unit(), channels,
                                          channel_layout)) {
      return false;
    }
  }

  DVLOG(2) << __FUNCTION__ << " Output channels: " << *channels
           << ", channel layout: " << ChannelLayoutToString(*channel_layout);
  return true;
}

static bool GetOutputDeviceChannelsAndLayout(AudioUnit audio_unit,
                                             int* channels,
                                             ChannelLayout* channel_layout) {
  // Attempt to retrieve the channel layout from the AudioUnit.
  std::unique_ptr<ScopedAudioChannelLayout> scoped_device_layout =
      AudioManagerApple::GetOutputDeviceChannelLayout(audio_unit);
  if (!scoped_device_layout) {
    DLOG(ERROR) << "Failed to retrieve output device channel layout.";
    return false;
  }
  AudioChannelLayout* device_layout = scoped_device_layout->layout();

  // There is no channel info for stereo, assume so for mono as well.
  if (device_layout->mNumberChannelDescriptions == 1 ||
      device_layout->mNumberChannelDescriptions == 2) {
    *channels = device_layout->mNumberChannelDescriptions;
    *channel_layout =
        *channels == 2 ? CHANNEL_LAYOUT_STEREO : CHANNEL_LAYOUT_MONO;
    return true;
  }

  *channels = 0;
  // use `CHANNEL_LAYOUT_DISCRETE` as the default layout if we can't
  // find out a matched one.
  *channel_layout = CHANNEL_LAYOUT_DISCRETE;

  std::vector<Channels> channels_to_match;
  for (UInt32 i = 0; i < device_layout->mNumberChannelDescriptions; i++) {
    AudioChannelLabel label =
        device_layout->mChannelDescriptions[i].mChannelLabel;
    if (label == kAudioChannelLabel_Unknown) {
      continue;
    }

    (*channels)++;

    Channels channel;
    if (AudioChannelLabelToChannel(label, &channel)) {
      channels_to_match.push_back(channel);
    }
  }

  if (*channels == 0 ||
      *channels != static_cast<int>(channels_to_match.size())) {
    return true;
  }

  for (int i = 0; i <= ChannelLayout::CHANNEL_LAYOUT_MAX; i++) {
    ChannelLayout layout = static_cast<ChannelLayout>(i);
    if (ChannelLayoutToChannelCount(layout) != *channels) {
      continue;
    }

    bool matched = true;
    for (const auto& channel : channels_to_match) {
      auto channel_order = ChannelOrder(layout, channel);
      if (channel_order == -1) {
        matched = false;
        break;
      }
    }

    if (matched) {
      *channel_layout = layout;
      return true;
    }
  }

  return true;
}

class AudioManagerMac::AudioPowerObserver : public base::PowerSuspendObserver {
 public:
  AudioPowerObserver()
      : is_suspending_(false),
        is_monitoring_(base::PowerMonitor::GetInstance()->IsInitialized()),
        num_resume_notifications_(0) {
    // The PowerMonitor requires significant setup (a CFRunLoop and preallocated
    // IO ports) so it's not available under unit tests.  See the OSX impl of
    // base::PowerMonitorDeviceSource for more details.
    if (!is_monitoring_) {
      return;
    }
    base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  }

  AudioPowerObserver(const AudioPowerObserver&) = delete;
  AudioPowerObserver& operator=(const AudioPowerObserver&) = delete;

  ~AudioPowerObserver() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!is_monitoring_) {
      return;
    }
    base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  }

  bool IsSuspending() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return is_suspending_;
  }

  size_t num_resume_notifications() const { return num_resume_notifications_; }

  bool ShouldDeferStreamStart() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    // Start() should be deferred if the system is in the middle of a suspend or
    // has recently started the process of resuming.
    return is_suspending_ || base::TimeTicks::Now() < earliest_start_time_;
  }

  bool IsOnBatteryPower() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return base::PowerMonitor::GetInstance()->IsOnBatteryPower();
  }

 private:
  void OnSuspend() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << "AudioPowerObserver::" << __FUNCTION__;
    is_suspending_ = true;
  }

  void OnResume() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << "AudioPowerObserver::" << __FUNCTION__;
    ++num_resume_notifications_;
    is_suspending_ = false;
    earliest_start_time_ =
        base::TimeTicks::Now() + base::Seconds(kStartDelayInSecsForPowerEvents);
  }

  bool is_suspending_;
  const bool is_monitoring_;
  base::TimeTicks earliest_start_time_;
  base::ThreadChecker thread_checker_;
  size_t num_resume_notifications_;
};

AudioManagerMac::AudioManagerMac(std::unique_ptr<AudioThread> audio_thread,
                                 AudioLogFactory* audio_log_factory)
    : AudioManagerApple(std::move(audio_thread), audio_log_factory),
      current_sample_rate_(0),
      current_output_device_(kAudioDeviceUnknown),
      in_shutdown_(false),
      weak_ptr_factory_(this) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  // PostTask since AudioManager creation may be on the startup path and this
  // may be slow.
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerMac::InitializeOnAudioThread,
                                weak_ptr_factory_.GetWeakPtr()));
}

AudioManagerMac::~AudioManagerMac() = default;

void AudioManagerMac::ShutdownOnAudioThread() {
  // We are now in shutdown mode. This flag disables MaybeChangeBufferSize()
  // and IncreaseIOBufferSizeIfPossible() which both touches native Core Audio
  // APIs and they can fail and disrupt tests during shutdown.
  in_shutdown_ = true;

  // Even if tasks to close the streams are enqueued, they would not run
  // leading to CHECKs getting hit in the destructor about open streams. Close
  // them explicitly here. crbug.com/608049.
  CloseAllInputStreams();
  CHECK(basic_input_streams_.empty());
  CHECK(low_latency_input_streams_.empty());

  // Deinitialize power observer on audio thread, since it's initialized on the
  // audio thread. Typically, constructor/destructor and
  // InitializeOnAudioThread/ShutdownOnAudioThread are all run on the main
  // thread, but this might not be true in testing.
  power_observer_.reset();

  AudioManagerBase::ShutdownOnAudioThread();
}

std::vector<AudioObjectID> AudioManagerMac::GetAllAudioDeviceIDs() {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  return core_audio_mac::GetAllAudioDeviceIDs();
}

std::vector<AudioObjectID> AudioManagerMac::GetRelatedNonBluetoothDeviceIDs(
    AudioObjectID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  return core_audio_mac::GetRelatedDeviceIDs(device_id);
}

std::vector<AudioObjectID> AudioManagerMac::GetRelatedBluetoothDeviceIDs(
    AudioObjectID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  std::vector<AudioObjectID> result_ids;

  // Get unique ID of input device which would be used to match with unique IDs
  // of all other devices.
  std::optional<std::string> input_unique_id = GetDeviceUniqueID(device_id);
  if (!input_unique_id) {
    return result_ids;
  }

  // Get the base name from the unique ID by removing :input/:output from it.
  // A bluetooth audio input device uniqueID is of the format
  // "F3-A2-14-A9-1D-F8:input", while the corresponding output device uniqueID
  // is of the format "F3-A2-14-A9-1D-F8:output".
  std::vector<std::string> trimmed_input_vector =
      SplitString(input_unique_id.value(), ":", base::TRIM_WHITESPACE,
                  base::SPLIT_WANT_NONEMPTY);
  if (trimmed_input_vector.empty()) {
    return result_ids;
  }
  std::string& trimmed_input_unique_id = trimmed_input_vector[0];

  // Iterate through all device IDs and match the unique IDs base to find the
  // related devices.
  for (const auto& id : GetAllAudioDeviceIDs()) {
    std::optional<std::string> unique_id = GetDeviceUniqueID(id);
    if (!unique_id) {
      continue;
    }

    std::vector<std::string> trimmed_vector =
        SplitString(unique_id.value(), ":", base::TRIM_WHITESPACE,
                    base::SPLIT_WANT_NONEMPTY);
    if (trimmed_vector.empty()) {
      continue;
    }

    std::string& trimmed_id = trimmed_vector[0];
    if (trimmed_id == trimmed_input_unique_id) {
      result_ids.push_back(id);
    }
  }
  return result_ids;
}

std::vector<AudioObjectID> AudioManagerMac::GetRelatedDeviceIDs(
    AudioObjectID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  std::optional<uint32_t> transport_type = GetDeviceTransportType(device_id);
  if (transport_type && *transport_type == kAudioDeviceTransportTypeBluetooth) {
    return GetRelatedBluetoothDeviceIDs(device_id);
  }
  return GetRelatedNonBluetoothDeviceIDs(device_id);
}

std::optional<std::string> AudioManagerMac::GetDeviceUniqueID(
    AudioObjectID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  return core_audio_mac::GetDeviceUniqueID(device_id);
}

std::optional<uint32_t> AudioManagerMac::GetDeviceTransportType(
    AudioObjectID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  return core_audio_mac::GetDeviceTransportType(device_id);
}

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

// static
void AudioManagerMac::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAudioDeviceInfo(true, device_names);
}

void AudioManagerMac::GetAudioOutputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAudioDeviceInfo(false, device_names);
}

AudioParameters AudioManagerMac::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           ChannelLayoutConfig::Stereo(), kLoopbackSampleRate,
                           ChooseBufferSize(true, kLoopbackSampleRate));
  }

  AudioDeviceID device = GetAudioDeviceIdByUId(true, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid device " << device_id;
    return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           ChannelLayoutConfig::Stereo(), kFallbackSampleRate,
                           ChooseBufferSize(true, kFallbackSampleRate));
  }

  int channels = 0;
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  if (GetInputDeviceChannels(device, &channels) && channels <= 2) {
    channel_layout_config = ChannelLayoutConfig::Guess(channels);
  } else {
    DLOG(ERROR) << "Failed to get the device channels, use stereo as default "
                << "for device " << device_id;
  }

  int sample_rate = HardwareSampleRateForDevice(device);
  if (!sample_rate) {
    sample_rate = kFallbackSampleRate;
  }

  // Due to the sharing of the input and output buffer sizes, we need to choose
  // the input buffer size based on the output sample rate.  See
  // http://crbug.com/154352.
  const int buffer_size = ChooseBufferSize(true, sample_rate);

  // TODO(grunell): query the native channel layout for the specific device.
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
      sample_rate, buffer_size,
      AudioParameters::HardwareCapabilities(
          GetMinAudioBufferSizeMacOS(limits::kMinAudioBufferSize, sample_rate),
          limits::kMaxAudioBufferSize));

  if (DeviceSupportsAmbientNoiseReduction(device)) {
    params.set_effects(AudioParameters::NOISE_SUPPRESSION);
  }

  // VoiceProcessingIO cannot be used on aggregate devices, since it creates an
  // aggregate device itself.  It also only runs in mono, but we allow upmixing
  // to stereo since we can't claim a device works either in stereo without echo
  // cancellation or mono with echo cancellation.
  if ((params.channel_layout() == CHANNEL_LAYOUT_MONO ||
       params.channel_layout() == CHANNEL_LAYOUT_STEREO) &&
      GetDeviceTransportType(device) != kAudioDeviceTransportTypeAggregate) {
    params.set_effects(params.effects() |
                       AudioParameters::EXPERIMENTAL_ECHO_CANCELLER);
  }

  return params;
}

std::string AudioManagerMac::GetAssociatedOutputDeviceID(
    const std::string& input_device_unique_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioObjectID input_device_id =
      GetAudioDeviceIdByUId(true, input_device_unique_id);
  if (input_device_id == kAudioObjectUnknown) {
    return std::string();
  }

  std::vector<AudioObjectID> related_device_ids =
      GetRelatedDeviceIDs(input_device_id);

  // Defined as a set as device IDs might be duplicated in
  // GetRelatedDeviceIDs().
  base::flat_set<AudioObjectID> related_output_device_ids;
  for (AudioObjectID device_id : related_device_ids) {
    if (core_audio_mac::GetNumStreams(device_id, false /* is_input */) > 0) {
      related_output_device_ids.insert(device_id);
    }
  }

  // Return the device ID if there is only one associated device.
  // When there are multiple associated devices, we currently do not have a way
  // to detect if a device (e.g. a digital output device) is actually connected
  // to an endpoint, so we cannot randomly pick a device.
  if (related_output_device_ids.size() == 1) {
    std::optional<std::string> related_unique_id =
        GetDeviceUniqueID(*related_output_device_ids.begin());
    if (related_unique_id) {
      return std::move(*related_unique_id);
    }
  }

  return std::string();
}

const char* AudioManagerMac::GetName() {
  return "Mac";
}

AudioOutputStream* AudioManagerMac::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return MakeLowLatencyOutputStream(params, std::string(), log_callback);
}

AudioOutputStream* AudioManagerMac::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool device_listener_first_init = false;
  // Lazily create the audio device listener on the first stream creation,
  // even if getting an audio device fails. Otherwise, if we have 0 audio
  // devices, the listener will never be initialized, and new valid devices
  // will never be detected.
  if (!output_device_listener_) {
    // NOTE: Use base::BindPostTaskToCurrentDefault() to ensure the callback is
    // always PostTask'd even if OSX calls us on the right thread.  Some
    // CoreAudio drivers will fire the callbacks during stream creation, leading
    // to re-entrancy issues otherwise.  See http://crbug.com/349604
    output_device_listener_ = AudioDeviceListenerMac::Create(
        base::BindPostTaskToCurrentDefault(
            base::BindRepeating(&AudioManagerMac::HandleDeviceChanges,
                                weak_ptr_factory_.GetWeakPtr())),
        /*monitor_sample_rate_changes=*/
        base::FeatureList::IsEnabled(kMonitorOutputSampleRateChangesMac),
        /*monitor_default_input=*/false,
        /*monitor_addition_removal=*/false,
        /*monitor_sources=*/false);
    device_listener_first_init = true;
  }

  AudioDeviceID device = GetAudioDeviceIdByUId(false, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Failed to open output device: " << device_id;
    return NULL;
  }

  // Only set the device and sample rate if we just initialized the device
  // listener.
  if (device_listener_first_init) {
    // Only set the current output device for the default device.
    if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
      current_output_device_ = device;
    }
    // Just use the current sample rate since we don't allow non-native sample
    // rates on OSX.
    current_sample_rate_ = params.sample_rate();
  }

  AUHALStream* stream = new AUHALStream(this, params, device, log_callback);
  output_streams_.insert(stream);
  return stream;
}

std::string AudioManagerMac::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return GetDefaultDeviceID(false /* is_input */);
}

std::string AudioManagerMac::GetDefaultInputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return GetDefaultDeviceID(true /* is_input */);
}

std::string AudioManagerMac::GetDefaultDeviceID(bool is_input) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceID device_id = kAudioObjectUnknown;
  if (!GetDefaultDevice(&device_id, is_input)) {
    return std::string();
  }

  const AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  CFStringRef device_uid = NULL;
  UInt32 size = sizeof(device_uid);
  OSStatus status = AudioObjectGetPropertyData(device_id, &property_address, 0,
                                               NULL, &size, &device_uid);
  if (status != kAudioHardwareNoError || !device_uid) {
    return std::string();
  }

  std::string ret(base::SysCFStringRefToUTF8(device_uid));
  CFRelease(device_uid);

  return ret;
}

AudioInputStream* AudioManagerMac::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  AudioInputStream* stream = new PCMQueueInAudioInputStream(this, params);
  basic_input_streams_.insert(stream);
  return stream;
}

AudioInputStream* AudioManagerMac::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    screen_capture_kit_swizzler_ = SwizzleScreenCaptureKit();

    return CreateSCKAudioInputStream(
        params, device_id, log_callback,
        base::BindRepeating(&AudioManagerBase::ReleaseInputStream,
                            base::Unretained(this)));
  }

  // Gets the AudioDeviceID that refers to the AudioInputDevice with the device
  // unique id. This AudioDeviceID is used to set the device for Audio Unit.
  AudioDeviceID audio_device_id = GetAudioDeviceIdByUId(true, device_id);
  if (audio_device_id == kAudioObjectUnknown) {
    return nullptr;
  }

  VoiceProcessingMode voice_processing_mode =
      (params.effects() & AudioParameters::ECHO_CANCELLER)
          ? VoiceProcessingMode::kEnabled
          : VoiceProcessingMode::kDisabled;

  auto* stream = new AUAudioInputStream(this, params, audio_device_id,
                                        log_callback, voice_processing_mode);
  low_latency_input_streams_.insert(stream);
  return stream;
}

AudioParameters AudioManagerMac::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const AudioDeviceID device = GetAudioDeviceIdByUId(false, output_device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid output device " << output_device_id;
    return input_params.IsValid()
               ? input_params
               : AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 ChannelLayoutConfig::Stereo(),
                                 kFallbackSampleRate,
                                 ChooseBufferSize(false, kFallbackSampleRate));
  }

  const bool has_valid_input_params = input_params.IsValid();
  const int hardware_sample_rate = HardwareSampleRateForDevice(device);

  // Allow pass through buffer sizes.  If concurrent input and output streams
  // exist, they will use the smallest buffer size amongst them.  As such, each
  // stream must be able to FIFO requests appropriately when this happens.
  int buffer_size;
  if (has_valid_input_params) {
    // Ensure the latency asked for is maintained, even if the sample rate is
    // changed here.
    const int scaled_buffer_size = input_params.frames_per_buffer() *
                                   hardware_sample_rate /
                                   input_params.sample_rate();
    // If passed in via the input_params we allow buffer sizes to go as
    // low as the the kMinAudioBufferSize, ignoring what
    // ChooseBufferSize() normally returns.
    buffer_size =
        std::min(static_cast<int>(limits::kMaxAudioBufferSize),
                 std::max(scaled_buffer_size,
                          static_cast<int>(limits::kMinAudioBufferSize)));
  } else {
    buffer_size = ChooseBufferSize(false, hardware_sample_rate);
  }

  int hardware_channels;
  ChannelLayout hardware_channel_layout;
  if (!GetOutputDeviceChannelsAndLayout(device, &hardware_channels,
                                        &hardware_channel_layout)) {
    hardware_channels = 2;
    hardware_channel_layout = CHANNEL_LAYOUT_STEREO;
  }

  // Use the input channel count and channel layout if possible.  Let OSX take
  // care of remapping the channels; this lets user specified channel layouts
  // work correctly.
  int output_channels = input_params.channels();
  ChannelLayout output_channel_layout = input_params.channel_layout();
  if (!has_valid_input_params || output_channels > hardware_channels) {
    output_channels = hardware_channels;
    output_channel_layout = hardware_channel_layout;
  }

  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      {output_channel_layout, output_channels}, hardware_sample_rate,
      buffer_size,
      AudioParameters::HardwareCapabilities(
          GetMinAudioBufferSizeMacOS(limits::kMinAudioBufferSize,
                                     hardware_sample_rate),
          limits::kMaxAudioBufferSize));
  return params;
}

void AudioManagerMac::InitializeOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  power_observer_ = std::make_unique<AudioPowerObserver>();
}

void AudioManagerMac::HandleDeviceChanges() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceID new_output_device;
  GetDefaultOutputDevice(&new_output_device);
  const int new_sample_rate = HardwareSampleRateForDevice(new_output_device);

  if (current_sample_rate_ == new_sample_rate &&
      current_output_device_ == new_output_device) {
    return;
  }

  DVLOG(1) << __func__
           << " device changed: " << (current_sample_rate_ != new_sample_rate)
           << " current sample rate: " << current_sample_rate_
           << " new sample rate: " << new_sample_rate;
  current_sample_rate_ = new_sample_rate;
  current_output_device_ = new_output_device;
  NotifyAllOutputDeviceChangeListeners();
}

int AudioManagerMac::ChooseBufferSize(bool is_input, int sample_rate) {
  // kMinAudioBufferSize is too small for the output side because
  // CoreAudio can get into under-run if the renderer fails delivering data
  // to the browser within the allowed time by the OS. The workaround is to
  // use 256 samples as the default output buffer size for sample rates
  // smaller than 96KHz.
  // TODO(xians): Remove this workaround after WebAudio supports user defined
  // buffer size.  See https://github.com/WebAudio/web-audio-api/issues/348
  // for details.
  int buffer_size =
      is_input ? limits::kMinAudioBufferSize : 2 * limits::kMinAudioBufferSize;
  const int user_buffer_size = GetUserBufferSize();
  buffer_size = user_buffer_size
                    ? user_buffer_size
                    : GetMinAudioBufferSizeMacOS(buffer_size, sample_rate);
  return buffer_size;
}

bool AudioManagerMac::IsSuspending() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->IsSuspending();
}

bool AudioManagerMac::ShouldDeferStreamStart() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->ShouldDeferStreamStart();
}
base::TimeDelta AudioManagerMac::GetDeferStreamStartTimeout() const {
  if (ShouldDeferStreamStart()) {
    return base::Seconds(AudioManagerMac::kStartDelayInSecsForPowerEvents);
  }
  return base::TimeDelta();
}

void AudioManagerMac::StopAmplitudePeakTrace() {
  TraceAmplitudePeak(/*trace_start=*/false);
}

double AudioManagerMac::GetMaxInputVolume(AudioDeviceID device_id) {
  // Verify that we have a valid device.
  if (device_id == kAudioObjectUnknown) {
    LOG(ERROR) << "Device ID is unknown";
    return 0.0;
  }

  // The master channel is 0, Left and right are channels 1 and 2.
  // Query if any of the master, left or right channels has volume control.
  for (int channel = 0; channel <= GetNumberOfChannelsForDevice(device_id);
       ++channel) {
    // If the volume is settable, the  valid volume range is [0.0, 1.0].
    if (IsVolumeSettableOnChannel(device_id, channel)) {
      return 1.0;
    }
  }

  // Volume control is not available for the audio stream.
  return 0.0;
}

bool AudioManagerMac::IsOnBatteryPower() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->IsOnBatteryPower();
}

size_t AudioManagerMac::GetNumberOfResumeNotifications() const {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return power_observer_->num_resume_notifications();
}

bool AudioManagerMac::MaybeChangeBufferSize(AudioDeviceID device_id,
                                            AudioUnit audio_unit,
                                            AudioUnitElement element,
                                            size_t desired_buffer_size) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (in_shutdown_) {
    DVLOG(1) << __FUNCTION__ << " Disabled since we are shutting down";
    return false;
  }
  const bool is_input = (element == 1);
  DVLOG(1) << __FUNCTION__ << " (id=0x" << std::hex << device_id
           << ", is_input=" << is_input << ", desired_buffer_size=" << std::dec
           << desired_buffer_size << ")";

  // Log the device name (and id) for debugging purposes.
  std::string device_name = GetAudioDeviceNameFromDeviceId(device_id, is_input);
  DVLOG(1) << __FUNCTION__ << " name: " << device_name << " (ID: 0x" << std::hex
           << device_id << ")";

  // Get the current size of the I/O buffer for the specified device. The
  // property is read on a global scope, hence using element 0. The default IO
  // buffer size on Mac OSX for OS X 10.9 and later is 512 audio frames.
  UInt32 buffer_size = 0;
  UInt32 property_size = sizeof(buffer_size);
  OSStatus result = AudioUnitGetProperty(
      audio_unit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
      AUElement::OUTPUT, &buffer_size, &property_size);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioUnitGetProperty(kAudioDevicePropertyBufferFrameSize) failed.";
    return false;
  }

  DVLOG(1) << __FUNCTION__ << " current IO buffer size: " << buffer_size;
  DVLOG(1) << __FUNCTION__ << " #output streams: " << output_streams_.size();
  DVLOG(1) << __FUNCTION__
           << " #input streams: " << low_latency_input_streams_.size();

  // Check if a buffer size change is required. If the caller asks for a
  // reduced size (|desired_buffer_size| < |buffer_size|), the new lower size
  // will be set. For larger buffer sizes, we have to perform some checks to
  // see if the size can actually be changed. If there is any other active
  // streams on the same device, either input or output, a larger size than
  // their requested buffer size can't be set. The reason is that an existing
  // stream can't handle buffer size larger than its requested buffer size.
  // See http://crbug.com/428706 for a reason why.

  if (buffer_size == desired_buffer_size) {
    return true;
  }

  if (desired_buffer_size > buffer_size) {
    // Do NOT set the buffer size if there is another output stream using
    // the same device with a smaller requested buffer size.
    // Note, for the caller stream, its requested_buffer_size() will be the same
    // as |desired_buffer_size|, so it won't return true due to comparing with
    // itself.
    for (auto* stream : output_streams_) {
      if (stream->device_id() == device_id &&
          stream->requested_buffer_size() < desired_buffer_size) {
        return true;
      }
    }

    // Do NOT set the buffer size if there is another input stream using
    // the same device with a smaller buffer size.
    for (auto* stream : low_latency_input_streams_) {
      if (stream->device_id() == device_id &&
          stream->requested_buffer_size() < desired_buffer_size) {
        return true;
      }
    }
  }

  // In this scope we know that the IO buffer size should be modified. But
  // first, verify that |desired_buffer_size| is within the valid range and
  // modify the desired buffer size if it is outside this range.
  // Note that, we have found that AudioUnitSetProperty(PropertyBufferFrameSize)
  // does in fact do this limitation internally and report noErr even if the
  // user tries to set an invalid size. As an example, asking for a size of
  // 4410 will on most devices be limited to 4096 without any further notice.
  UInt32 minimum = buffer_size;
  UInt32 maximum = buffer_size;
  result = GetIOBufferFrameSizeRange(device_id, is_input, &minimum, &maximum);
  if (result != noErr) {
    // OS error is logged in GetIOBufferFrameSizeRange().
    return false;
  }
  DVLOG(1) << __FUNCTION__ << " valid IO buffer size range: [" << minimum
           << ", " << maximum << "]";
  buffer_size = desired_buffer_size;
  if (buffer_size < minimum) {
    buffer_size = minimum;
  } else if (buffer_size > maximum) {
    buffer_size = maximum;
  }
  DVLOG(1) << "validated desired buffer size: " << buffer_size;

  // Set new (and valid) I/O buffer size for the specified device. The property
  // is set on a global scope, hence using element 0.
  result = AudioUnitSetProperty(audio_unit, kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Global, 0, &buffer_size,
                                sizeof(buffer_size));
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitSetProperty(kAudioDevicePropertyBufferFrameSize) failed.  "
      << "Size:: " << buffer_size;
  DVLOG_IF(1, result == noErr)
      << __FUNCTION__ << " IO buffer size changed to: " << buffer_size;
  // Store the currently used (after a change) I/O buffer frame size.
  return result == noErr;
}

bool AudioManagerMac::DeviceSupportsAmbientNoiseReduction(
    AudioDeviceID device_id) {
  return AudioObjectHasProperty(device_id, &kNoiseReductionPropertyAddress);
}

bool AudioManagerMac::SuppressNoiseReduction(AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(DeviceSupportsAmbientNoiseReduction(device_id));
  NoiseReductionState& state = device_noise_reduction_states_[device_id];
  if (state.suppression_count == 0) {
    UInt32 initially_enabled = 0;
    UInt32 size = sizeof(initially_enabled);
    OSStatus result =
        AudioObjectGetPropertyData(device_id, &kNoiseReductionPropertyAddress,
                                   0, nullptr, &size, &initially_enabled);
    if (result != noErr) {
      return false;
    }

    if (initially_enabled) {
      const UInt32 disable = 0;
      result =
          AudioObjectSetPropertyData(device_id, &kNoiseReductionPropertyAddress,
                                     0, nullptr, sizeof(disable), &disable);
      if (result != noErr) {
        OSSTATUS_DLOG(WARNING, result)
            << "Failed to disable ambient noise reduction for device: "
            << std::hex << device_id;
      }
      state.initial_state = NoiseReductionState::ENABLED;
    } else {
      state.initial_state = NoiseReductionState::DISABLED;
    }
  }

  // Only increase the counter if suppression succeeded or is already active.
  ++state.suppression_count;
  return true;
}

void AudioManagerMac::UnsuppressNoiseReduction(AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  NoiseReductionState& state = device_noise_reduction_states_[device_id];
  DCHECK_NE(state.suppression_count, 0);
  --state.suppression_count;
  if (state.suppression_count == 0) {
    if (state.initial_state == NoiseReductionState::ENABLED) {
      const UInt32 enable = 1;
      OSStatus result =
          AudioObjectSetPropertyData(device_id, &kNoiseReductionPropertyAddress,
                                     0, nullptr, sizeof(enable), &enable);
      if (result != noErr) {
        OSSTATUS_DLOG(WARNING, result)
            << "Failed to re-enable ambient noise reduction for device: "
            << std::hex << device_id;
      }
    }
  }
}

void AudioManagerMac::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  CHECK(stream);

  auto it = output_streams_.find(static_cast<AUHALStream*>(stream));
  if (it != output_streams_.end()) {
    output_streams_.erase(it);
  }

  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerMac::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  auto stream_it = basic_input_streams_.find(stream);
  if (stream_it != basic_input_streams_.end()) {
    basic_input_streams_.erase(stream_it);
  } else {
    auto it = low_latency_input_streams_.find(
        static_cast<AUAudioInputStream*>(stream));
    if (it != low_latency_input_streams_.end()) {
      low_latency_input_streams_.erase(
          static_cast<AUAudioInputStream*>(stream));
    }
  }

  AudioManagerBase::ReleaseInputStream(stream);
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerMac>(std::move(audio_thread),
                                           audio_log_factory);
}

// static
bool AudioManagerMac::IsVolumeSettableOnChannel(AudioDeviceID device_id,
                                                int channel) {
  Boolean is_settable = false;
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput,
      static_cast<UInt32>(channel)};
  OSStatus result =
      AudioObjectIsPropertySettable(device_id, &property_address, &is_settable);
  return (result == noErr) ? is_settable : false;
}

void AudioManagerMac::SetInputVolume(AudioDeviceID device_id, double volume) {
  CHECK_GE(volume, 0.0);
  CHECK_LE(volume, 1.0);

  // Verify that we have a valid device.
  if (device_id == kAudioObjectUnknown) {
    LOG(ERROR) << "Device ID is unknown";
    return;
  }

  Float32 volume_float32 = static_cast<Float32>(volume);
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMain};

  // Try to set the volume for master volume channel.
  if (IsVolumeSettableOnChannel(device_id, kAudioObjectPropertyElementMain)) {
    OSStatus result =
        AudioObjectSetPropertyData(device_id, &property_address, 0, nullptr,
                                   sizeof(volume_float32), &volume_float32);
    if (result != noErr) {
      DLOG(WARNING) << "Failed to set volume to " << volume_float32;
    }
    return;
  }

  // The master channel is 0, Left and right are channels 1 and 2.
  // There is no master volume control, try to set volume for each channel.
  [[maybe_unused]] int successful_channels = 0;
  for (int channel = 1; channel <= GetNumberOfChannelsForDevice(device_id);
       ++channel) {
    property_address.mElement = static_cast<UInt32>(channel);
    if (IsVolumeSettableOnChannel(device_id, channel)) {
      OSStatus result =
          AudioObjectSetPropertyData(device_id, &property_address, 0, NULL,
                                     sizeof(volume_float32), &volume_float32);
      if (result == noErr) {
        ++successful_channels;
      }
    }
  }

  DLOG_IF(WARNING, successful_channels == 0)
      << "Failed to set volume to " << volume_float32;
}

double AudioManagerMac::GetInputVolume(AudioDeviceID device_id) {
  // Verify that we have a valid device.
  if (device_id == kAudioObjectUnknown) {
    LOG(ERROR) << "Device ID is unknown";
    return 0.0;
  }

  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMain};

  if (AudioObjectHasProperty(device_id, &property_address)) {
    // The device supports master volume control, get the volume from the
    // master channel.
    Float32 volume_float32 = 0.0;
    UInt32 size = sizeof(volume_float32);
    OSStatus result = AudioObjectGetPropertyData(
        device_id, &property_address, 0, nullptr, &size, &volume_float32);
    if (result == noErr) {
      return static_cast<double>(volume_float32);
    }
    return 0.0;
  } else {
    // There is no master volume control, try to get the average volume of
    // all the channels.
    Float32 volume_float32 = 0.0;
    int successful_channels = 0;
    for (int i = 1; i <= GetNumberOfChannelsForDevice(device_id); ++i) {
      property_address.mElement = static_cast<UInt32>(i);
      if (AudioObjectHasProperty(device_id, &property_address)) {
        Float32 channel_volume = 0;
        UInt32 size = sizeof(channel_volume);
        OSStatus result = AudioObjectGetPropertyData(
            device_id, &property_address, 0, nullptr, &size, &channel_volume);
        if (result == noErr) {
          volume_float32 += channel_volume;
          ++successful_channels;
        }
      }
    }

    // Get the average volume of the channels.
    if (successful_channels != 0) {
      return static_cast<double>(volume_float32 / successful_channels);
    }
  }

  DLOG(WARNING) << "Failed to get volume";
  return 0.0;
}

bool AudioManagerMac::IsInputMuted(AudioDeviceID device_id) {
  // Verify that we have a valid device.
  DCHECK_NE(device_id, kAudioObjectUnknown) << "Device ID is unknown";

  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMain};

  if (!AudioObjectHasProperty(device_id, &property_address)) {
    DLOG(ERROR) << "Device does not support checking master mute state";
    return false;
  }

  UInt32 muted = 0;
  UInt32 size = sizeof(muted);
  OSStatus result = AudioObjectGetPropertyData(device_id, &property_address, 0,
                                               nullptr, &size, &muted);
  DLOG_IF(WARNING, result != noErr) << "Failed to get mute state";
  return result == noErr && muted != 0;
}

int AudioManagerMac::HardwareSampleRateForDevice(AudioDeviceID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  Float64 nominal_sample_rate;
  UInt32 info_size = sizeof(nominal_sample_rate);

  static const AudioObjectPropertyAddress kNominalSampleRateAddress = {
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  OSStatus result =
      AudioObjectGetPropertyData(device_id, &kNominalSampleRateAddress, 0, 0,
                                 &info_size, &nominal_sample_rate);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Could not get default sample rate for device: " << device_id
        << ", returning fallback sample rate " << kFallbackSampleRate;
    return kFallbackSampleRate;
  }

  return static_cast<int>(nominal_sample_rate);
}

// static
AudioDeviceID AudioManagerMac::FindFirstOutputSubdevice(
    AudioDeviceID aggregate_device_id) {
  const AudioObjectPropertyAddress property_address = {
      kAudioAggregateDevicePropertyFullSubDeviceList,
      kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
  base::apple::ScopedCFTypeRef<CFArrayRef> subdevices;
  UInt32 size = sizeof(subdevices);
  OSStatus result = AudioObjectGetPropertyData(
      aggregate_device_id, &property_address, 0 /* inQualifierDataSize */,
      nullptr /* inQualifierData */, &size, subdevices.InitializeInto());

  if (result != noErr) {
    OSSTATUS_LOG(WARNING, result)
        << "Failed to read property "
        << kAudioAggregateDevicePropertyFullSubDeviceList << " for device "
        << aggregate_device_id;
    return kAudioObjectUnknown;
  }

  AudioDeviceID output_subdevice_id = kAudioObjectUnknown;
  DCHECK_EQ(CFGetTypeID(subdevices.get()), CFArrayGetTypeID());
  const CFIndex count = CFArrayGetCount(subdevices.get());
  for (CFIndex i = 0; i < count; ++i) {
    CFStringRef value = base::apple::CFCast<CFStringRef>(
        CFArrayGetValueAtIndex(subdevices.get(), i));
    if (value) {
      std::string uid = base::SysCFStringRefToUTF8(value);
      output_subdevice_id = AudioManagerMac::GetAudioDeviceIdByUId(false, uid);
      if (output_subdevice_id != kAudioObjectUnknown &&
          core_audio_mac::GetNumStreams(output_subdevice_id, false) > 0) {
        return output_subdevice_id;
      }
    }
  }

  return kAudioObjectUnknown;
}

OSStatus AudioManagerMac::GetInputDeviceStreamFormat(
    AudioUnit audio_unit,
    AudioStreamBasicDescription* input_format) {
  DCHECK(audio_unit);
  UInt32 property_size = sizeof(*input_format);
  // Get the audio stream data format on the input scope of the input element
  // since it is connected to the current input device.
  return AudioUnitGetProperty(audio_unit, kAudioUnitProperty_StreamFormat,
                              kAudioUnitScope_Input, AUElement::INPUT,
                              input_format, &property_size);
}

// static
int AudioManagerMac::GetNumberOfChannelsForDevice(AudioDeviceID device_id) {
  // The master channel is 0, Left and right are channels 1 and 2. And the
  // master channel is not counted.

  // Get the stream format, to be able to read the number of channels.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeInput,
      kAudioObjectPropertyElementMain};
  AudioStreamBasicDescription stream_format;
  UInt32 size = sizeof(stream_format);
  OSStatus result = AudioObjectGetPropertyData(device_id, &property_address, 0,
                                               nullptr, &size, &stream_format);
  if (result != noErr) {
    DLOG(WARNING) << "Could not get stream format";
    return 0;
  }

  return static_cast<int>(stream_format.mChannelsPerFrame);
}

}  // namespace media
