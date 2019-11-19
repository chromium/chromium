// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_manager_mac.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/optional.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/mac/audio_auhal_mac.h"
#include "media/audio/mac/audio_input_mac.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/audio/mac/core_audio_util_mac.h"
#include "media/audio/mac/coreaudio_dispatch_override.h"
#include "media/audio/mac/scoped_audio_unit.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/mac/audio_latency_mac.h"
#include "media/base/media_switches.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample-rate on most Apple hardware.
static const int kFallbackSampleRate = 44100;

static bool GetDeviceChannels(AudioUnit audio_unit,
                              AUElement element,
                              int* channels);

// Helper method to construct AudioObjectPropertyAddress structure given
// property selector and scope. The property element is always set to
// kAudioObjectPropertyElementMaster.
static AudioObjectPropertyAddress GetAudioObjectPropertyAddress(
    AudioObjectPropertySelector selector,
    bool is_input) {
  AudioObjectPropertyScope scope = is_input ? kAudioObjectPropertyScopeInput
                                            : kAudioObjectPropertyScopeOutput;
  AudioObjectPropertyAddress property_address = {
      selector, scope, kAudioObjectPropertyElementMaster};
  return property_address;
}

static const AudioObjectPropertyAddress kNoiseReductionPropertyAddress = {
    'nzca', kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMaster};

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
    kAudioObjectPropertyScopeGlobal,            // mScope
    kAudioObjectPropertyElementMaster           // mElement
  };
  UInt32 output_device_id_size = static_cast<UInt32>(sizeof(output_device_id));
  OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                            &property_address,
                                            0,     // inQualifierDataSize
                                            NULL,  // inQualifierData
                                            &output_device_id_size,
                                            &output_device_id);
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

    if (!is_valid_for_direction)
      continue;

    base::Optional<std::string> unique_id =
        core_audio_mac::GetDeviceUniqueID(device_id);
    if (!unique_id)
      continue;

    base::Optional<std::string> label =
        core_audio_mac::GetDeviceLabel(device_id, is_input);
    if (!label)
      continue;

    // Filter out aggregate devices, e.g. those that get created by using
    // kAudioUnitSubType_VoiceProcessingIO.
    if (core_audio_mac::IsPrivateAggregateDevice(device_id))
      continue;

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
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  AudioDeviceID audio_device_id = kAudioObjectUnknown;
  UInt32 device_size = sizeof(audio_device_id);
  OSStatus result = -1;

  if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
    // Default Device.
    property_address.mSelector = is_input ?
        kAudioHardwarePropertyDefaultInputDevice :
        kAudioHardwarePropertyDefaultOutputDevice;

    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &property_address,
                                        0,
                                        0,
                                        &device_size,
                                        &audio_device_id);
  } else {
    // Non-default device.
    base::ScopedCFTypeRef<CFStringRef> uid(
        base::SysUTF8ToCFStringRef(device_id));
    AudioValueTranslation value;
    value.mInputData = &uid;
    value.mInputDataSize = sizeof(CFStringRef);
    value.mOutputData = &audio_device_id;
    value.mOutputDataSize = device_size;
    UInt32 translation_size = sizeof(AudioValueTranslation);

    property_address.mSelector = kAudioHardwarePropertyDeviceForUID;
    result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &property_address,
                                        0,
                                        0,
                                        &translation_size,
                                        &value);
  }

  if (result) {
    OSSTATUS_DLOG(WARNING, result) << "Unable to query device " << device_id
                                   << " for AudioDeviceID";
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
  pa.mElement = kAudioObjectPropertyElementMaster;

  UInt32 size = sizeof(*device);
  OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &pa, 0,
                                               0, &size, device);
  if ((result != kAudioHardwareNoError) || (*device == kAudioDeviceUnknown)) {
    DLOG(ERROR) << "Error getting default AudioDevice.";
    return false;
  }
  return true;
}

bool AudioManagerMac::GetDefaultOutputDevice(AudioDeviceID* device) {
  return GetDefaultDevice(device, false);
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
                                   scope, kAudioObjectPropertyElementMaster};

  UInt32 size;
  OSStatus result = AudioObjectGetPropertyDataSize(device, &pa, 0, 0, &size);
  if (result != noErr || !size)
    return false;

  std::unique_ptr<uint8_t[]> list_storage(new uint8_t[size]);
  AudioBufferList* buffer_list =
      reinterpret_cast<AudioBufferList*>(list_storage.get());

  result = AudioObjectGetPropertyData(device, &pa, 0, 0, &size, buffer_list);
  if (result != noErr)
    return false;

  // Determine number of channels based on the AudioBufferList.
  // |mNumberBuffers] is the  number of interleaved channels in the buffer.
  // If the number is 1, the buffer is noninterleaved.
  *channels = 0;
  for (UInt32 i = 0; i < buffer_list->mNumberBuffers; ++i)
    *channels += buffer_list->mBuffers[i].mNumberChannels;

  DVLOG(1) << (scope == kAudioDevicePropertyScopeInput ? "Input" : "Output")
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

// Returns the channel layout for |device| as provided by the AudioUnit attached
// to that device matching |element|. Returns true if the count could be pulled
// from the AudioUnit successfully, false otherwise.
static bool GetDeviceChannels(AudioDeviceID device,
                              AUElement element,
                              int* channels) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  CHECK(channels);

  // For input, get the channel count directly from the AudioUnit's stream
  // format.
  // TODO(https://crbug.com/796163): Find out if we can use channel layout on
  // input element, or confirm that we can't.
  if (element == AUElement::INPUT) {
    ScopedAudioUnit au(device, element);
    if (!au.is_valid())
      return false;

    if (!GetAudioUnitStreamFormatChannelCount(au.audio_unit(), element,
                                              channels)) {
      return false;
    }

    DVLOG(1) << "Input channels: " << *channels;
    return true;
  }

  // For output, use the channel layout to determine channel count.
  DCHECK(element == AUElement::OUTPUT);

  // If the device has more channels than possible for layouts to express, use
  // the total count of channels on the device; as of this writing, macOS will
  // only return up to 8 channels in any layout. To allow WebAudio to work with
  // > 8 channel devices, we must use the total channel count instead of the
  // channel count of the preferred layout.
  int total_channel_count = 0;
  if (GetDeviceTotalChannelCount(device,
                                 element == AUElement::OUTPUT
                                     ? kAudioDevicePropertyScopeOutput
                                     : kAudioDevicePropertyScopeInput,
                                 &total_channel_count) &&
      total_channel_count > kMaxConcurrentChannels) {
    *channels = total_channel_count;
    return true;
  }

  ScopedAudioUnit au(device, element);
  if (!au.is_valid())
    return false;

  return GetDeviceChannels(au.audio_unit(), element, channels);
}

static bool GetDeviceChannels(AudioUnit audio_unit,
                              AUElement element,
                              int* channels) {
  // Attempt to retrieve the channel layout from the AudioUnit.
  //
  // Note: We don't use kAudioDevicePropertyPreferredChannelLayout on the device
  // because it is not available on all devices.
  UInt32 size;
  Boolean writable;
  OSStatus result = AudioUnitGetPropertyInfo(
      audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Output,
      element, &size, &writable);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to get property info for AudioUnit channel layout.";
  }

  std::unique_ptr<uint8_t[]> layout_storage(new uint8_t[size]);
  AudioChannelLayout* layout =
      reinterpret_cast<AudioChannelLayout*>(layout_storage.get());

  result =
      AudioUnitGetProperty(audio_unit, kAudioUnitProperty_AudioChannelLayout,
                           kAudioUnitScope_Output, element, layout, &size);
  if (result != noErr) {
    OSSTATUS_LOG(ERROR, result) << "Failed to get AudioUnit channel layout.";
    return false;
  }

  // We don't want to have to know about all channel layout tags, so force OSX
  // to give us the channel descriptions from the bitmap or tag if necessary.
  const AudioChannelLayoutTag tag = layout->mChannelLayoutTag;
  if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
    const bool is_bitmap = tag == kAudioChannelLayoutTag_UseChannelBitmap;
    const AudioFormatPropertyID fa =
        is_bitmap ? kAudioFormatProperty_ChannelLayoutForBitmap
                  : kAudioFormatProperty_ChannelLayoutForTag;

    if (is_bitmap) {
      result = AudioFormatGetPropertyInfo(fa, sizeof(UInt32),
                                          &layout->mChannelBitmap, &size);
    } else {
      result = AudioFormatGetPropertyInfo(fa, sizeof(AudioChannelLayoutTag),
                                          &tag, &size);
    }
    if (result != noErr || !size) {
      OSSTATUS_DLOG(ERROR, result)
          << "Failed to get AudioFormat property info, size=" << size;
      return false;
    }

    layout_storage.reset(new uint8_t[size]);
    layout = reinterpret_cast<AudioChannelLayout*>(layout_storage.get());
    if (is_bitmap) {
      result = AudioFormatGetProperty(fa, sizeof(UInt32),
                                      &layout->mChannelBitmap, &size, layout);
    } else {
      result = AudioFormatGetProperty(fa, sizeof(AudioChannelLayoutTag), &tag,
                                      &size, layout);
    }
    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result) << "Failed to get AudioFormat property.";
      return false;
    }
  }

  // There is no channel info for stereo, assume so for mono as well.
  if (layout->mNumberChannelDescriptions <= 2) {
    *channels = layout->mNumberChannelDescriptions;
  } else {
    *channels = 0;
    for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; ++i) {
      if (layout->mChannelDescriptions[i].mChannelLabel !=
          kAudioChannelLabel_Unknown)
        (*channels)++;
    }
  }

  DVLOG(1) << "Output channels: " << *channels;
  return true;
}

class AudioManagerMac::AudioPowerObserver : public base::PowerObserver {
 public:
  AudioPowerObserver()
      : is_suspending_(false),
        is_monitoring_(base::PowerMonitor::IsInitialized()),
        num_resume_notifications_(0) {
    // The PowerMonitor requires significant setup (a CFRunLoop and preallocated
    // IO ports) so it's not available under unit tests.  See the OSX impl of
    // base::PowerMonitorDeviceSource for more details.
    if (!is_monitoring_)
      return;
    base::PowerMonitor::AddObserver(this);
  }

  ~AudioPowerObserver() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!is_monitoring_)
      return;
    base::PowerMonitor::RemoveObserver(this);
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
    return base::PowerMonitor::IsOnBatteryPower();
  }

 private:
  void OnSuspend() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << "OnSuspend";
    is_suspending_ = true;
  }

  void OnResume() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << "OnResume";
    ++num_resume_notifications_;
    is_suspending_ = false;
    earliest_start_time_ = base::TimeTicks::Now() +
        base::TimeDelta::FromSeconds(kStartDelayInSecsForPowerEvents);
  }

  bool is_suspending_;
  const bool is_monitoring_;
  base::TimeTicks earliest_start_time_;
  base::ThreadChecker thread_checker_;
  size_t num_resume_notifications_;

  DISALLOW_COPY_AND_ASSIGN(AudioPowerObserver);
};

AudioManagerMac::AudioManagerMac(std::unique_ptr<AudioThread> audio_thread,
                                 AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
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

bool AudioManagerMac::HasAudioOutputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultOutputDevice);
}

bool AudioManagerMac::HasAudioInputDevices() {
  return HasAudioHardware(kAudioHardwarePropertyDefaultInputDevice);
}

// static
int AudioManagerMac::HardwareSampleRateForDevice(AudioDeviceID device_id) {
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  Float64 nominal_sample_rate;
  UInt32 info_size = sizeof(nominal_sample_rate);

  static const AudioObjectPropertyAddress kNominalSampleRateAddress = {
      kAudioDevicePropertyNominalSampleRate,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster
  };
  OSStatus result = AudioObjectGetPropertyData(device_id,
                                               &kNominalSampleRateAddress,
                                               0,
                                               0,
                                               &info_size,
                                               &nominal_sample_rate);
  if (result != noErr) {
    OSSTATUS_DLOG(WARNING, result)
        << "Could not get default sample rate for device: " << device_id;
    return 0;
  }

  return static_cast<int>(nominal_sample_rate);
}

// static
int AudioManagerMac::HardwareSampleRate() {
  // Determine the default output device's sample-rate.
  AudioDeviceID device_id = kAudioObjectUnknown;
  if (!GetDefaultOutputDevice(&device_id))
    return kFallbackSampleRate;

  return HardwareSampleRateForDevice(device_id);
}

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
  AudioDeviceID device = GetAudioDeviceIdByUId(true, device_id);
  if (device == kAudioObjectUnknown) {
    DLOG(ERROR) << "Invalid device " << device_id;
    return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           CHANNEL_LAYOUT_STEREO, kFallbackSampleRate,
                           ChooseBufferSize(true, kFallbackSampleRate));
  }

  int channels = 0;
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  if (GetDeviceChannels(device, AUElement::INPUT, &channels) && channels <= 2) {
    channel_layout = GuessChannelLayout(channels);
  } else {
    DLOG(ERROR) << "Failed to get the device channels, use stereo as default "
                << "for device " << device_id;
  }

  int sample_rate = HardwareSampleRateForDevice(device);
  if (!sample_rate)
    sample_rate = kFallbackSampleRate;

  // Due to the sharing of the input and output buffer sizes, we need to choose
  // the input buffer size based on the output sample rate.  See
  // http://crbug.com/154352.
  const int buffer_size = ChooseBufferSize(true, sample_rate);

  // TODO(grunell): query the native channel layout for the specific device.
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, sample_rate,
      buffer_size,
      AudioParameters::HardwareCapabilities(
          GetMinAudioBufferSizeMacOS(limits::kMinAudioBufferSize, sample_rate),
          limits::kMaxAudioBufferSize));

  if (DeviceSupportsAmbientNoiseReduction(device)) {
    params.set_effects(AudioParameters::NOISE_SUPPRESSION);
  }

  // VoiceProcessingIO is only supported on MacOS 10.12 and cannot be used on
  // aggregate devices, since it creates an aggregate device itself.  It also
  // only runs in mono, but we allow upmixing to stereo since we can't claim a
  // device works either in stereo without echo cancellation or mono with echo
  // cancellation.
  if (base::mac::IsAtLeastOS10_12() &&
      (params.channel_layout() == CHANNEL_LAYOUT_MONO ||
       params.channel_layout() == CHANNEL_LAYOUT_STEREO) &&
      core_audio_mac::GetDeviceTransportType(device) !=
          kAudioDeviceTransportTypeAggregate) {
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
  if (input_device_id == kAudioObjectUnknown)
    return std::string();

  std::vector<AudioObjectID> related_device_ids =
      core_audio_mac::GetRelatedDeviceIDs(input_device_id);

  std::vector<AudioObjectID> related_output_device_ids;
  for (AudioObjectID device_id : related_device_ids) {
    if (core_audio_mac::GetNumStreams(device_id, false /* is_input */) > 0)
      related_output_device_ids.push_back(device_id);
  }

  // Return the device ID if there is only one associated device.
  // When there are multiple associated devices, we currently do not have a way
  // to detect if a device (e.g. a digital output device) is actually connected
  // to an endpoint, so we cannot randomly pick a device.
  if (related_output_device_ids.size() == 1) {
    base::Optional<std::string> related_unique_id =
        core_audio_mac::GetDeviceUniqueID(related_output_device_ids[0]);
    if (related_unique_id)
      return std::move(*related_unique_id);
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
    // NOTE: Use BindToCurrentLoop() to ensure the callback is always PostTask'd
    // even if OSX calls us on the right thread.  Some CoreAudio drivers will
    // fire the callbacks during stream creation, leading to re-entrancy issues
    // otherwise.  See http://crbug.com/349604
    output_device_listener_.reset(
        new AudioDeviceListenerMac(BindToCurrentLoop(base::Bind(
            &AudioManagerMac::HandleDeviceChanges, base::Unretained(this)))));
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
    if (AudioDeviceDescription::IsDefaultDevice(device_id))
      current_output_device_ = device;
    // Just use the current sample rate since we don't allow non-native sample
    // rates on OSX.
    current_sample_rate_ = params.sample_rate();
  }

  AUHALStream* stream = new AUHALStream(this, params, device, log_callback);
  output_streams_.push_back(stream);
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
  if (!GetDefaultDevice(&device_id, is_input))
    return std::string();

  const AudioObjectPropertyAddress property_address = {
    kAudioDevicePropertyDeviceUID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
  };
  CFStringRef device_uid = NULL;
  UInt32 size = sizeof(device_uid);
  OSStatus status = AudioObjectGetPropertyData(device_id,
                                               &property_address,
                                               0,
                                               NULL,
                                               &size,
                                               &device_uid);
  if (status != kAudioHardwareNoError || !device_uid)
    return std::string();

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
  basic_input_streams_.push_back(stream);
  return stream;
}

AudioInputStream* AudioManagerMac::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
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
  low_latency_input_streams_.push_back(stream);
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
                                 CHANNEL_LAYOUT_STEREO, kFallbackSampleRate,
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
  if (!GetDeviceChannels(device, AUElement::OUTPUT, &hardware_channels))
    hardware_channels = 2;

  // Use the input channel count and channel layout if possible.  Let OSX take
  // care of remapping the channels; this lets user specified channel layouts
  // work correctly.
  int output_channels = input_params.channels();
  ChannelLayout channel_layout = input_params.channel_layout();
  if (!has_valid_input_params || output_channels > hardware_channels) {
    output_channels = hardware_channels;
    channel_layout = GuessChannelLayout(output_channels);
    if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED)
      channel_layout = CHANNEL_LAYOUT_DISCRETE;
  }

  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      hardware_sample_rate, buffer_size,
      AudioParameters::HardwareCapabilities(
          GetMinAudioBufferSizeMacOS(limits::kMinAudioBufferSize,
                                     hardware_sample_rate),
          limits::kMaxAudioBufferSize));
  params.set_channels_for_discrete(output_channels);
  return params;
}

void AudioManagerMac::InitializeOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  InitializeCoreAudioDispatchOverride();
  power_observer_.reset(new AudioPowerObserver());
}

void AudioManagerMac::HandleDeviceChanges() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const int new_sample_rate = HardwareSampleRate();
  AudioDeviceID new_output_device;
  GetDefaultOutputDevice(&new_output_device);

  if (current_sample_rate_ == new_sample_rate &&
      current_output_device_ == new_output_device) {
    return;
  }

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
                                            size_t desired_buffer_size,
                                            bool* size_was_changed,
                                            size_t* io_buffer_frame_size) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (in_shutdown_) {
    DVLOG(1) << "Disabled since we are shutting down";
    return false;
  }
  const bool is_input = (element == 1);
  DVLOG(1) << "MaybeChangeBufferSize(id=0x" << std::hex << device_id
           << ", is_input=" << is_input << ", desired_buffer_size=" << std::dec
           << desired_buffer_size << ")";

  *size_was_changed = false;
  *io_buffer_frame_size = 0;

  // Log the device name (and id) for debugging purposes.
  std::string device_name = GetAudioDeviceNameFromDeviceId(device_id, is_input);
  DVLOG(1) << "name: " << device_name << " (ID: 0x" << std::hex << device_id
           << ")";

  // Get the current size of the I/O buffer for the specified device. The
  // property is read on a global scope, hence using element 0. The default IO
  // buffer size on Mac OSX for OS X 10.9 and later is 512 audio frames.
  UInt32 buffer_size = 0;
  UInt32 property_size = sizeof(buffer_size);
  OSStatus result = AudioUnitGetProperty(
      audio_unit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
      0, &buffer_size, &property_size);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioUnitGetProperty(kAudioDevicePropertyBufferFrameSize) failed.";
    return false;
  }
  // Store the currently used (not changed yet) I/O buffer frame size.
  *io_buffer_frame_size = buffer_size;

  DVLOG(1) << "current IO buffer size: " << buffer_size;
  DVLOG(1) << "#output streams: " << output_streams_.size();
  DVLOG(1) << "#input streams: " << low_latency_input_streams_.size();

  // Check if a buffer size change is required. If the caller asks for a
  // reduced size (|desired_buffer_size| < |buffer_size|), the new lower size
  // will be set. For larger buffer sizes, we have to perform some checks to
  // see if the size can actually be changed. If there is any other active
  // streams on the same device, either input or output, a larger size than
  // their requested buffer size can't be set. The reason is that an existing
  // stream can't handle buffer size larger than its requested buffer size.
  // See http://crbug.com/428706 for a reason why.

  if (buffer_size == desired_buffer_size)
    return true;

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
  DVLOG(1) << "valid IO buffer size range: [" << minimum << ", " << maximum
           << "]";
  buffer_size = desired_buffer_size;
  if (buffer_size < minimum)
    buffer_size = minimum;
  else if (buffer_size > maximum)
    buffer_size = maximum;
  DVLOG(1) << "validated desired buffer size: " << buffer_size;

  // Set new (and valid) I/O buffer size for the specified device. The property
  // is set on a global scope, hence using element 0.
  result = AudioUnitSetProperty(audio_unit, kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Global, 0, &buffer_size,
                                sizeof(buffer_size));
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioUnitSetProperty(kAudioDevicePropertyBufferFrameSize) failed.  "
      << "Size:: " << buffer_size;
  *size_was_changed = (result == noErr);
  DVLOG_IF(1, result == noErr) << "IO buffer size changed to: " << buffer_size;
  // Store the currently used (after a change) I/O buffer frame size.
  *io_buffer_frame_size = buffer_size;

  // If the size was changed, update the actual output buffer size used for the
  // given device ID.
  if (!is_input && (result == noErr)) {
    output_io_buffer_size_map_[device_id] = buffer_size;
  }

  return (result == noErr);
}

// static
base::TimeDelta AudioManagerMac::GetHardwareLatency(
    AudioUnit audio_unit,
    AudioDeviceID device_id,
    AudioObjectPropertyScope scope,
    int sample_rate) {
  if (!audio_unit || device_id == kAudioObjectUnknown) {
    DLOG(WARNING) << "Audio unit object is NULL or device ID is unknown";
    return base::TimeDelta();
  }

  // Get audio unit latency.
  Float64 audio_unit_latency_sec = 0.0;
  UInt32 size = sizeof(audio_unit_latency_sec);
  OSStatus result = AudioUnitGetProperty(audio_unit, kAudioUnitProperty_Latency,
                                         kAudioUnitScope_Global, 0,
                                         &audio_unit_latency_sec, &size);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "Could not get audio unit latency";

  // Get audio device latency.
  AudioObjectPropertyAddress property_address = {
      kAudioDevicePropertyLatency, scope, kAudioObjectPropertyElementMaster};
  UInt32 device_latency_frames = 0;
  size = sizeof(device_latency_frames);
  result = AudioObjectGetPropertyData(device_id, &property_address, 0, nullptr,
                                      &size, &device_latency_frames);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "Could not get audio device latency.";

  // Retrieve stream ids and take the stream latency from the first stream.
  // There may be multiple streams with different latencies, but since we're
  // likely using this delay information for a/v sync we must choose one of
  // them; Apple recommends just taking the first entry.
  //
  // TODO(dalecurtis): Refactor all these "get data size" + "get data" calls
  // into a common utility function that just returns a std::unique_ptr.
  UInt32 stream_latency_frames = 0;
  property_address.mSelector = kAudioDevicePropertyStreams;
  result = AudioObjectGetPropertyDataSize(device_id, &property_address, 0,
                                          nullptr, &size);
  if (result == noErr && size >= sizeof(AudioStreamID)) {
    std::unique_ptr<uint8_t[]> stream_id_storage(new uint8_t[size]);
    AudioStreamID* stream_ids =
        reinterpret_cast<AudioStreamID*>(stream_id_storage.get());
    result = AudioObjectGetPropertyData(device_id, &property_address, 0,
                                        nullptr, &size, stream_ids);
    if (result == noErr) {
      property_address.mSelector = kAudioStreamPropertyLatency;
      size = sizeof(stream_latency_frames);
      result =
          AudioObjectGetPropertyData(stream_ids[0], &property_address, 0,
                                     nullptr, &size, &stream_latency_frames);
      OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
          << "Could not get stream latency for stream #0.";
    } else {
      OSSTATUS_DLOG(WARNING, result)
          << "Could not get audio device stream ids.";
    }
  } else {
    OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
        << "Could not get audio device stream ids size.";
  }

  return base::TimeDelta::FromSecondsD(audio_unit_latency_sec) +
         AudioTimestampHelper::FramesToTime(
             device_latency_frames + stream_latency_frames, sample_rate);
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
    if (result != noErr)
      return false;

    if (initially_enabled) {
      const UInt32 disable = 0;
      OSStatus result =
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

void AudioManagerMac::IncreaseIOBufferSizeIfPossible(AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "IncreaseIOBufferSizeIfPossible(id=0x" << std::hex << device_id
           << ")";
  if (in_shutdown_) {
    DVLOG(1) << "Disabled since we are shutting down";
    return;
  }

  // Start by getting the actual I/O buffer size. Then scan all active output
  // streams using the specified |device_id| and find the minimum requested
  // buffer size. In addition, store a reference to the audio unit of the first
  // output stream using |device_id|.
  // All active output streams use the same actual I/O buffer size given
  // a unique device ID.
  // TODO(henrika): it would also be possible to use AudioUnitGetProperty(...,
  // kAudioDevicePropertyBufferFrameSize,...) instead of caching the actual
  // buffer size but I have chosen to use the map instead to avoid possibly
  // expensive Core Audio API calls and the risk of failure when asking while
  // closing a stream.
  // TODO(http://crbug.com/961629): There seems to be bugs in the caching.
  const size_t actual_size =
      output_io_buffer_size_map_.find(device_id) !=
              output_io_buffer_size_map_.end()
          ? output_io_buffer_size_map_[device_id]
          : 0;  // This leads to trying to update the buffer size below.
  AudioUnit audio_unit;
  size_t min_requested_size = std::numeric_limits<std::size_t>::max();
  for (auto* stream : output_streams_) {
    if (stream->device_id() == device_id) {
      if (min_requested_size == std::numeric_limits<std::size_t>::max()) {
        // Store reference to the first audio unit using the specified ID.
        audio_unit = stream->audio_unit();
      }
      if (stream->requested_buffer_size() < min_requested_size)
        min_requested_size = stream->requested_buffer_size();
      DVLOG(1) << "requested:" << stream->requested_buffer_size()
               << " actual: " << actual_size;
    }
  }

  if (min_requested_size == std::numeric_limits<std::size_t>::max()) {
    DVLOG(1) << "No action since there is no active stream for given device id";
    return;
  }

  // It is only possible to revert to a larger buffer size if the lowest
  // requested is not in use. Example: if the actual I/O buffer size is 256 and
  // at least one output stream has asked for 256 as its buffer size, we can't
  // start using a larger I/O buffer size.
  DCHECK_GE(min_requested_size, actual_size);
  if (min_requested_size == actual_size) {
    DVLOG(1) << "No action since lowest possible size is already in use: "
             << actual_size;
    return;
  }

  // It should now be safe to increase the I/O buffer size to a new (higher)
  // value using the |min_requested_size|. Doing so will save system resources.
  // All active output streams with the same |device_id| are affected by this
  // change but it is only required to apply the change to one of the streams.
  // We ignore the result from MaybeChangeBufferSize(). Logging is done in that
  // function and it could fail if the device was removed during the operation.
  DVLOG(1) << "min_requested_size: " << min_requested_size;
  bool size_was_changed = false;
  size_t io_buffer_frame_size = 0;
  MaybeChangeBufferSize(device_id, audio_unit, 0, min_requested_size,
                        &size_was_changed, &io_buffer_frame_size);
}

bool AudioManagerMac::AudioDeviceIsUsedForInput(AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!basic_input_streams_.empty()) {
    // For Audio Queues and in the default case (Mac OS X), the audio comes
    // from the system’s default audio input device as set by a user in System
    // Preferences.
    AudioDeviceID default_id;
    GetDefaultDevice(&default_id, true);
    if (default_id == device_id)
      return true;
  }

  // Each low latency streams has its own device ID.
  for (auto* stream : low_latency_input_streams_) {
    if (stream->device_id() == device_id)
      return true;
  }
  return false;
}

void AudioManagerMac::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerMac::ReleaseOutputStreamUsingRealDevice(
    AudioOutputStream* stream,
    AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DVLOG(1) << "Closing output stream with id=0x" << std::hex << device_id;
  DVLOG(1) << "requested_buffer_size: "
           << static_cast<AUHALStream*>(stream)->requested_buffer_size();

  // Start by closing down the specified output stream.
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);

  // Prevent attempt to alter buffer size if the released stream was the last
  // output stream.
  if (output_streams_.empty())
    return;

  // If the audio device exists (i.e. has not been removed from the system) and
  // is not used for input, see if it is possible to increase the IO buffer size
  // (saves power) given the remaining output audio streams and their buffer
  // size requirements.
  // TODO(grunell): When closing several idle streams
  // (AudioOutputDispatcherImpl::CloseIdleStreams), we should ideally only
  // update the buffer size once after closing all those streams.
  std::vector<AudioObjectID> device_ids =
      core_audio_mac::GetAllAudioDeviceIDs();
  const bool device_exists = std::find(device_ids.begin(), device_ids.end(),
                                       device_id) != device_ids.end();
  if (device_exists && !AudioDeviceIsUsedForInput(device_id))
    IncreaseIOBufferSizeIfPossible(device_id);
}

void AudioManagerMac::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  auto stream_it = std::find(basic_input_streams_.begin(),
                             basic_input_streams_.end(),
                             stream);
  if (stream_it == basic_input_streams_.end())
    low_latency_input_streams_.remove(static_cast<AUAudioInputStream*>(stream));
  else
    basic_input_streams_.erase(stream_it);

  AudioManagerBase::ReleaseInputStream(stream);
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerMac>(std::move(audio_thread),
                                           audio_log_factory);
}

}  // namespace media
