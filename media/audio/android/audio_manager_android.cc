// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "build/android_buildflags.h"
#include "media/audio/android/aaudio_bluetooth_output.h"
#include "media/audio/android/aaudio_input.h"
#include "media/audio/android/aaudio_output.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"
#include "media/audio/android/audio_track_output_stream.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/localized_strings.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_OPENSLES)
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/AudioManagerAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using media::android::AudioDevice;
using media::android::AudioDeviceId;
using media::android::AudioDeviceType;
using media::android::IntToAudioDeviceType;
using JniAudioDevice = media::AudioManagerAndroid::JniAudioDevice;

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 10;

const int kDefaultInputBufferSize = 1024;
const int kDefaultOutputBufferSize = 2048;

class JniDelegateImpl : public AudioManagerAndroid::JniDelegate {
 public:
  explicit JniDelegateImpl(AudioManagerAndroid* audio_manager)
      : j_audio_manager_(Java_AudioManagerAndroid_createAudioManagerAndroid(
            AttachCurrentThread(),
            reinterpret_cast<jlong>(audio_manager))) {
    Java_AudioManagerAndroid_init(AttachCurrentThread(), j_audio_manager_);
  }

  ~JniDelegateImpl() override {
    Java_AudioManagerAndroid_close(AttachCurrentThread(), j_audio_manager_);
    j_audio_manager_.Reset();
  }

  std::vector<JniAudioDevice> GetDevices(bool inputs) override {
    ScopedJavaLocalRef<jobjectArray> j_devices =
        Java_AudioManagerAndroid_getDevices(AttachCurrentThread(),
                                            j_audio_manager_, inputs);
    std::vector<JniAudioDevice> devices;
    for (ScopedJavaLocalRef<jobject> j_device :
         j_devices.ReadElements<jobject>()) {
      devices.emplace_back(
          Java_AudioDevice_id(AttachCurrentThread(), j_device),
          Java_AudioDevice_name(AttachCurrentThread(), j_device),
          Java_AudioDevice_type(AttachCurrentThread(), j_device));
    }
    return devices;
  }

  std::optional<std::vector<JniAudioDevice>> GetCommunicationDevices()
      override {
    ScopedJavaLocalRef<jobjectArray> j_devices =
        Java_AudioManagerAndroid_getCommunicationDevices(AttachCurrentThread(),
                                                         j_audio_manager_);
    if (j_devices.is_null()) {
      return std::nullopt;
    }

    std::vector<JniAudioDevice> devices;
    for (ScopedJavaLocalRef<jobject> j_device :
         j_devices.ReadElements<jobject>()) {
      devices.emplace_back(
          Java_AudioDevice_id(AttachCurrentThread(), j_device),
          Java_AudioDevice_name(AttachCurrentThread(), j_device),
          Java_AudioDevice_type(AttachCurrentThread(), j_device));
    }
    return devices;
  }

  int GetMinInputFrameSize(int sample_rate, int channels) override {
    return Java_AudioManagerAndroid_getMinInputFrameSize(AttachCurrentThread(),
                                                         sample_rate, channels);
  }

  bool AcousticEchoCancelerIsAvailable() override {
    return Java_AudioManagerAndroid_acousticEchoCancelerIsAvailable(
        AttachCurrentThread());
  }

  base::TimeDelta GetOutputLatency() override {
    return base::Milliseconds(Java_AudioManagerAndroid_getOutputLatency(
        AttachCurrentThread(), j_audio_manager_));
  }

  void SetCommunicationAudioModeOn(bool on) override {
    Java_AudioManagerAndroid_setCommunicationAudioModeOn(AttachCurrentThread(),
                                                         j_audio_manager_, on);
  }

  bool SetCommunicationDevice(std::string_view device_id) override {
    // Send the unique device ID to the Java audio manager and make the
    // device switch. Provide an empty string to the Java audio manager
    // if the default device is selected.
    ScopedJavaLocalRef<jstring> j_device_id =
        base::android::ConvertUTF8ToJavaString(
            AttachCurrentThread(),
            device_id == AudioDeviceDescription::kDefaultDeviceId
                ? std::string()
                : device_id);
    return Java_AudioManagerAndroid_setCommunicationDevice(
        AttachCurrentThread(), j_audio_manager_, j_device_id);
  }

  bool IsBluetoothScoOn() override {
    return Java_AudioManagerAndroid_isBluetoothScoOn(AttachCurrentThread(),
                                                     j_audio_manager_);
  }

  void MaybeSetBluetoothScoState(bool state) override {
    DVLOG(1) << __func__ << "(" << base::ToString(state) << ")";
    return Java_AudioManagerAndroid_maybeSetBluetoothScoState(
        AttachCurrentThread(), j_audio_manager_, state);
  }

  int GetNativeOutputSampleRate() override {
    return Java_AudioManagerAndroid_getNativeOutputSampleRate(
        AttachCurrentThread(), j_audio_manager_);
  }

  bool IsAudioLowLatencySupported() override {
    return Java_AudioManagerAndroid_isAudioLowLatencySupported(
        AttachCurrentThread(), j_audio_manager_);
  }

  int GetAudioLowLatencyOutputFrameSize() override {
    return Java_AudioManagerAndroid_getAudioLowLatencyOutputFrameSize(
        AttachCurrentThread(), j_audio_manager_);
  }

  int GetMinOutputFrameSize(int sample_rate, int channels) override {
    return Java_AudioManagerAndroid_getMinOutputFrameSize(
        AttachCurrentThread(), sample_rate, channels);
  }

  AudioParameters::Format GetHdmiOutputEncodingFormats() override {
    return Java_AudioManagerAndroid_getHdmiOutputEncodingFormats(
        AttachCurrentThread());
  }

  int GetLayoutWithMaxChannels() override {
    return Java_AudioManagerAndroid_getLayoutWithMaxChannels(
        AttachCurrentThread(), j_audio_manager_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_audio_manager_;
};

void AddDefaultDevice(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(AudioDeviceName::CreateDefault());
}

std::string GetFallbackDeviceNameForType(AudioDeviceType type) {
  switch (type) {
    case AudioDeviceType::kBuiltinEarpiece:
    case AudioDeviceType::kBuiltinSpeaker:
    case AudioDeviceType::kBuiltinSpeakerSafe:
      return GetLocalizedStringUTF8(
          MessageId::INTERNAL_SPEAKER_AUDIO_DEVICE_NAME);
    case AudioDeviceType::kBuiltinMic:
      return GetLocalizedStringUTF8(MessageId::INTERNAL_MIC_AUDIO_DEVICE_NAME);
    case AudioDeviceType::kWiredHeadset:
    case AudioDeviceType::kWiredHeadphones:
      return GetLocalizedStringUTF8(
          MessageId::WIRED_HEADPHONES_AUDIO_DEVICE_NAME);
    case AudioDeviceType::kBluetoothSco:
    case AudioDeviceType::kBluetoothA2dp:
    case AudioDeviceType::kBleHeadset:
    case AudioDeviceType::kBleSpeaker:
    case AudioDeviceType::kBleBroadcast:
    case AudioDeviceType::kHearingAid:
      return GetLocalizedStringUTF8(MessageId::BLUETOOTH_AUDIO_DEVICE_NAME);
    case AudioDeviceType::kUsbDevice:
    case AudioDeviceType::kUsbAccessory:
    case AudioDeviceType::kUsbHeadset:
      return GetLocalizedStringUTF8(MessageId::USB_AUDIO_DEVICE_NAME);
    case AudioDeviceType::kHdmi:
    case AudioDeviceType::kHdmiArc:
    case AudioDeviceType::kHdmiEarc:
      return GetLocalizedStringUTF8(MessageId::HDMI_AUDIO_DEVICE_NAME);
    case AudioDeviceType::kUnknown:
    case AudioDeviceType::kLineAnalog:
    case AudioDeviceType::kLineDigital:
    case AudioDeviceType::kDock:
    case AudioDeviceType::kFm:
    case AudioDeviceType::kFmTuner:
    case AudioDeviceType::kTvTuner:
    case AudioDeviceType::kTelephony:
    case AudioDeviceType::kAuxLine:
    case AudioDeviceType::kIp:
    case AudioDeviceType::kBus:
    case AudioDeviceType::kRemoteSubmix:
    case AudioDeviceType::kEchoReference:
    case AudioDeviceType::kDockAnalog:
    case AudioDeviceType::kMultichannelGroup:
      return GetLocalizedStringUTF8(MessageId::GENERIC_AUDIO_DEVICE_NAME);
  }
}

// Utility function used by `GetDeviceNames()` to find an A2DP/SCO device pair,
// if present, and combine it into a single A2DP device with an associated SCO
// device.
void CombineBluetoothClassicDevices(
    std::vector<std::pair<AudioDeviceId, AudioDevice>>& devices,
    AudioDeviceNames* device_names) {
  constexpr auto is_a2dp_predicate = [](const auto& pair) -> bool {
    return pair.second.GetType() == AudioDeviceType::kBluetoothA2dp;
  };
  constexpr auto is_sco_predicate = [](const auto& pair) -> bool {
    return pair.second.GetType() == AudioDeviceType::kBluetoothSco;
  };

  // It is assumed that only up to 1 of each of these device types will be
  // present. If this assumption is invalidated, we can't determine associations
  // between A2DP and SCO devices, and it is uncertain how to handle them.
  // Here, we choose to not do any combining in this case.
  if (std::ranges::count_if(devices, is_a2dp_predicate) > 1 ||
      std::ranges::count_if(devices, is_sco_predicate) > 1) {
    LOG(WARNING) << "Found multiple A2DP or SCO output devices";
    return;
  }

  auto a2dp_device = std::ranges::find_if(devices, is_a2dp_predicate);
  if (a2dp_device == devices.end()) {
    return;
  }
  auto sco_device = std::ranges::find_if(devices, is_sco_predicate);
  if (sco_device == devices.end()) {
    return;
  }

  a2dp_device->second.SetAssociatedScoDeviceId(sco_device->second.GetId());
  device_names->remove_if([sco_device](AudioDeviceName& name) {
    return AudioDeviceId::Parse(name.unique_id) == sco_device->second.GetId();
  });
  devices.erase(sco_device);
}

bool UseAAudioOutput() {
  if (!__builtin_available(android AAUDIO_MIN_API, *)) {
    return false;
  }

  return base::FeatureList::IsEnabled(features::kUseAAudioDriver);
}

bool UseAAudioInput() {
  if (!__builtin_available(android AAUDIO_MIN_API, *)) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(features::kUseAAudioInput)) {
    return false;
  }

  if (auto* info = base::android::BuildInfo::GetInstance()) {
    // Disable AAudio input on Unisoc devices running Android 11 and below due
    // to missing/broken echo cancellation. See https://crbug.com/344607452.
    if (base::StartsWith(info->board(), "ums",
                         base::CompareCase::INSENSITIVE_ASCII) &&
        info->sdk_int() < base::android::SDK_VERSION_S) {
      return false;
    }
  }

  return true;
}

bool UseAAudioPerStreamDeviceSelection() {
  return UseAAudioInput() && UseAAudioOutput() &&
         base::FeatureList::IsEnabled(
             features::kAAudioPerStreamDeviceSelection);
}

}  // namespace

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerAndroid>(std::move(audio_thread),
                                               audio_log_factory);
}

JniAudioDevice::JniAudioDevice(int id,
                               std::optional<std::string> name,
                               int type) {
  this->id = id;
  this->name = std::move(name);
  this->type = type;
}

JniAudioDevice::JniAudioDevice(const JniAudioDevice&) = default;

JniAudioDevice& JniAudioDevice::operator=(const JniAudioDevice&) = default;

JniAudioDevice::JniAudioDevice(JniAudioDevice&&) = default;

JniAudioDevice& JniAudioDevice::operator=(JniAudioDevice&&) = default;

JniAudioDevice::~JniAudioDevice() = default;

AudioManagerAndroid::AudioManagerAndroid(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      communication_mode_is_on_(false),
      output_volume_override_set_(false),
      output_volume_override_(0) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerAndroid::~AudioManagerAndroid() = default;

void AudioManagerAndroid::InitializeIfNeeded() {
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&AudioManagerAndroid::GetJniDelegate),
                     base::Unretained(this)));
}

void AudioManagerAndroid::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();

  // Destroy the JNI delegate here because the Java AudioManagerAndroid can only
  // be closed on the audio thread.
  jni_delegate_.reset();
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return true;
}

void AudioManagerAndroid::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (UseAAudioPerStreamDeviceSelection()) {
    GetDeviceNames(device_names, AudioDeviceDirection::kInput);
    return;
  }

  // Android devices in general do not have robust support for specifying
  // devices individually per input or output stream, and as such
  // `AAudioPerStreamDeviceSelection` is usually disabled. Instead, if a
  // specific device is requested, we set a single input/output pair (a.k.a. a
  // "communication device") to be used for streams. Note that it is possible
  // for a communication device to be an output-only device. In these cases,
  // the framework seems to choose some other available input device for
  // communication streams. It's not clear whether this is a real issue,
  // considering how long this code has been around for...
  //
  // For compatibility with Android R-, which predates the concept of
  // Android communication devices, the externally exposed devices are
  // "synthetic" devices which abstract away the internal device IDs and
  // manufacturer-given names provided by the Android framework (e.g.
  // "Bluetooth headset" instead of "FooBuds Pro 2.0"):
  // * On Android S+, these devices correspond to actual communication
  // devices.
  // * On Android R-, these devices don't correspond to devices from a list,
  // but each one can be controlled via appropriate Android API calls, e.g.
  // AudioManager#startBluetoothSco() for Bluetooth.
  GetCommunicationDeviceNames(device_names);
}

void AudioManagerAndroid::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (UseAAudioPerStreamDeviceSelection()) {
    GetDeviceNames(device_names, AudioDeviceDirection::kOutput);
    return;
  }

  // Android devices in general do not have robust support for specifying
  // devices individually per input or output stream, and as such
  // `AAudioPerStreamDeviceSelection` is usually disabled. In these
  // situations, if a specific device is requested, we set a single
  // input/output pair (a.k.a. a "communication device") to be used for
  // streams system-wide.
  //
  // We've only returned "default" here for quite some time, relying on output
  // device selection being controlled by input device selection (see
  // `GetAudioInputDeviceNames`). Populating this list with other devices has
  // prevented confusion for users; it would've given them the option to set a
  // different input and output device, which wouldn't actually work. However,
  // since communication devices on Android are technically output devices for
  // which an input device is automatically chosen, it could be more
  // appropriate to invert the input and output device lists.
  AddDefaultDevice(device_names);
}

void AudioManagerAndroid::GetDeviceNames(AudioDeviceNames* device_names,
                                         AudioDeviceDirection direction) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(device_names);

  // Always add default device parameters as first element.
  DCHECK(device_names->empty());
  AddDefaultDevice(device_names);

  std::vector<JniAudioDevice> j_devices =
      GetJniDelegate().GetDevices(direction == AudioDeviceDirection::kInput);

  // This container is later converted to a `base::flat_map`, but it starts out
  // as an `std::vector` in order to avoid the O(n) insertion time of
  // `base::flat_map`.
  std::vector<std::pair<AudioDeviceId, AudioDevice>> devices;

  // Populate `devices` and `device_names`.
  for (auto& j_device : j_devices) {
    std::optional<AudioDeviceId> device_id =
        AudioDeviceId::NonDefault(j_device.id);
    if (!device_id.has_value()) {
      LOG(WARNING) << "Unexpectedly received device with default ID";
      continue;
    }

    std::optional<AudioDeviceType> device_type =
        IntToAudioDeviceType(j_device.type);
    if (!device_type.has_value()) {
      LOG(WARNING) << "No device type matching integer value: "
                   << j_device.type;
      device_type = AudioDeviceType::kUnknown;
    }

    std::string device_name = j_device.name.value_or(
        GetFallbackDeviceNameForType(device_type.value()));
    std::string device_id_string =
        base::NumberToString(device_id->ToAAudioDeviceId());
    device_names->emplace_back(std::move(device_name),
                               std::move(device_id_string));

    AudioDevice device(device_id.value(), device_type.value());
    devices.emplace_back(std::move(device_id).value(), std::move(device));
  }

  // If a Bluetooth SCO output device and a Bluetooth A2DP output device are
  // both present, remove the SCO device from `devices` and `device_names`, and
  // instead make it "associated" with the A2DP device.
  if (direction == AudioDeviceDirection::kOutput) {
    CombineBluetoothClassicDevices(devices, device_names);
  }

  switch (direction) {
    case AudioDeviceDirection::kInput:
      input_device_cache_ = base::flat_map(devices);
      break;
    case AudioDeviceDirection::kOutput:
      output_device_cache_ = base::flat_map(devices);
      break;
  }
}

void AudioManagerAndroid::GetCommunicationDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(device_names);

  // Always add default device parameters as first element.
  DCHECK(device_names->empty());
  AddDefaultDevice(device_names);

  std::optional<std::vector<JniAudioDevice>> j_devices =
      GetJniDelegate().GetCommunicationDevices();
  if (!j_devices) {
    // Most probable reason for an `std::nullopt` result here is that the
    // process lacks MODIFY_AUDIO_SETTINGS or RECORD_AUDIO permissions.
    return;
  }

  for (auto& j_device : j_devices.value()) {
    // The device name should always be one of the predefined communication
    // device names and so it should always be present.
    CHECK(j_device.name);

    std::string device_id_string = base::NumberToString(j_device.id);
    device_names->emplace_back(std::move(j_device.name).value(),
                               std::move(device_id_string));
  }
}

const AudioManagerAndroid::DeviceCache& AudioManagerAndroid::GetDeviceCache(
    AudioDeviceDirection direction) const {
  switch (direction) {
    case AudioDeviceDirection::kInput:
      return input_device_cache_;
    case AudioDeviceDirection::kOutput:
      return output_device_cache_;
  }
}

std::optional<AudioDevice> AudioManagerAndroid::GetDeviceForAAudioStream(
    std::string_view id_string,
    AudioDeviceDirection direction) {
  if (!UseAAudioPerStreamDeviceSelection()) {
    return AudioDevice::Default();
  }

  AudioDeviceId id =
      AudioDeviceId::Parse(id_string).value_or(AudioDeviceId::Default());
  if (id.IsDefault()) {
    return AudioDevice::Default();
  }

  const DeviceCache& devices = GetDeviceCache(direction);
  auto device = devices.find(id);
  if (device == devices.end()) {
    return std::nullopt;
  }
  return device->second;
}

AudioParameters AudioManagerAndroid::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Use mono as preferred number of input channels on Android to save
  // resources. Using mono also avoids a driver issue seen on Samsung
  // Galaxy S3 and S4 devices. See http://crbug.com/256851 for details.
  const ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Mono();

  const int sample_rate = GetJniDelegate().GetNativeOutputSampleRate();

  int buffer_size = GetJniDelegate().GetMinInputFrameSize(
      sample_rate, channel_layout_config.channels());
  if (buffer_size <= 0) {
    buffer_size = kDefaultInputBufferSize;
  }
  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size) {
    buffer_size = user_buffer_size;
  }

  AudioParameters::PlatformEffectsMask effects =
      GetJniDelegate().AcousticEchoCancelerIsAvailable()
          ? AudioParameters::ECHO_CANCELLER
          : AudioParameters::NO_EFFECTS;

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size);
  params.set_effects(effects);
  return params;
}

const std::string_view AudioManagerAndroid::GetName() {
  return "Android";
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioOutputStream* stream = AudioManagerBase::MakeAudioOutputStream(
      params, device_id, AudioManager::LogCallback());
  if (stream) {
    output_streams_.insert(static_cast<MuteableAudioOutputStream*>(stream));
  }
  return stream;
}

AudioInputStream* AudioManagerAndroid::MakeAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool has_input_streams = !HasNoAudioInputStreams();
  AudioInputStream* stream = AudioManagerBase::MakeAudioInputStream(
      params, device_id, AudioManager::LogCallback());
  // Avoid changing the communication mode if there are existing input streams.
  if (!stream || has_input_streams || UseAAudioPerStreamDeviceSelection()) {
    return stream;
  }

  // By default, the audio manager for Android creates streams intended for
  // real-time VoIP sessions and therefore sets the audio mode to
  // MODE_IN_COMMUNICATION. However, the user might have asked for a special
  // mode where all audio input processing is disabled, and if that is the case
  // we avoid changing the mode.

  // To ensure proper audio routing when a Bluetooth microphone is in use,
  // Android's audio manager must switch the output from TYPE_BLUETOOTH_A2DP to
  // TYPE_BLUETOOTH_SCO. This switch is triggered by setting the audio mode to
  // MODE_IN_COMMUNICATION. Failing to activate communication mode can result
  // in audio being routed incorrectly, leading to no sound output from the
  // Bluetooth headset.
  bool force_communication_mode = false;
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  force_communication_mode = GetJniDelegate().IsBluetoothScoOn();
#endif
  if (params.effects() != AudioParameters::NO_EFFECTS ||
      force_communication_mode) {
    communication_mode_is_on_ = true;
    GetJniDelegate().SetCommunicationAudioModeOn(true);
  }
  return stream;
}

void AudioManagerAndroid::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  output_streams_.erase(static_cast<MuteableAudioOutputStream*>(stream));
  if (__builtin_available(android AAUDIO_MIN_API, *)) {
    bluetooth_output_streams_.erase(stream);
  }

  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerAndroid::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioManagerBase::ReleaseInputStream(stream);

  // Restore the audio mode which was used before the first communication-
  // mode stream was created.
  if (HasNoAudioInputStreams() && communication_mode_is_on_) {
    communication_mode_is_on_ = false;
    GetJniDelegate().SetCommunicationAudioModeOn(false);
  }
}

AudioOutputStream* AudioManagerAndroid::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (__builtin_available(android AAUDIO_MIN_API, *)) {
    if (UseAAudioOutput()) {
      return new AAudioOutputStream(
          this, params, AudioDevice::Default(), AAUDIO_USAGE_MEDIA,
          base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                              base::Unretained(this),
                              /*trace_start=*/false));
    }
  }
#if BUILDFLAG(USE_OPENSLES)
  return new OpenSLESOutputStream(this, params, SL_ANDROID_STREAM_MEDIA);
#else
  return nullptr;
#endif
}

AudioOutputStream* AudioManagerAndroid::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (__builtin_available(android AAUDIO_MIN_API, *)) {
    if (UseAAudioOutput()) {
      DLOG_IF(WARNING, !UseAAudioPerStreamDeviceSelection() &&
                           !AudioDeviceDescription::IsDefaultDevice(device_id))
          << "Non-default output device requested for output communication "
             "stream.";

      std::optional<AudioDevice> device =
          GetDeviceForAAudioStream(device_id, AudioDeviceDirection::kOutput);
      if (!device.has_value()) {
        return nullptr;
      }

      const aaudio_usage_t usage = communication_mode_is_on_
                                       ? AAUDIO_USAGE_VOICE_COMMUNICATION
                                       : AAUDIO_USAGE_MEDIA;

      auto peak_detected_cb = base::BindRepeating(
          &AudioManager::TraceAmplitudePeak, base::Unretained(this),
          /*trace_start=*/false);
      if (device->GetAssociatedScoDevice().has_value()) {
        // Use a specialized stream implementation to handle "combined" A2DP/SCO
        // devices.

        // TODO(crbug.com/405955144): Set `use_sco_device` based on the SCO
        // state as reported by the system in order to handle SCO management by
        // other apps.
        auto* stream = new AAudioBluetoothOutputStream(
            *this, params, std::move(device).value(),
            /*use_sco_device=*/!input_streams_requiring_sco_.empty(), usage,
            peak_detected_cb);
        bluetooth_output_streams_.insert(stream);
        return stream;
      }

      return new AAudioOutputStream(this, params, std::move(device).value(),
                                    usage, peak_detected_cb);
    }
  }

  // Set stream type which matches the current system-wide audio mode used by
  // the Android audio manager.
#if BUILDFLAG(USE_OPENSLES)
  const SLint32 stream_type = communication_mode_is_on_
                                  ? SL_ANDROID_STREAM_VOICE
                                  : SL_ANDROID_STREAM_MEDIA;

  return new OpenSLESOutputStream(this, params, stream_type);
#else
  return nullptr;
#endif
}

AudioOutputStream* AudioManagerAndroid::MakeBitstreamOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(params.IsBitstreamFormat());
  return new AudioTrackOutputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());

  if (__builtin_available(android AAUDIO_MIN_API, *)) {
    if (UseAAudioInput()) {
      std::optional<AudioDevice> device =
          GetDeviceForAAudioStream(device_id, AudioDeviceDirection::kInput);
      if (!device.has_value()) {
        return nullptr;
      }
      return new AAudioInputStream(this, params, std::move(device).value());
    }
  }

#if BUILDFLAG(USE_OPENSLES)
  return new OpenSLESInputStream(this, params);
#else
  return nullptr;
#endif
}

AudioInputStream* AudioManagerAndroid::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DVLOG(1) << "MakeLowLatencyInputStream: " << params.effects();
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  DLOG_IF(ERROR, device_id.empty()) << "Invalid device ID!";

  if (!UseAAudioPerStreamDeviceSelection()) {
    // Use the device ID to select the correct communication device. If the
    // default device is requested, a communication device will be chosen based
    // on an internal selection scheme. Note that a communication device is an
    // output device that the system associates with an input device, and this
    // selection switches the device used for all input and output streams with
    // communication usage set.
    if (!GetJniDelegate().SetCommunicationDevice(device_id)) {
      LOG(ERROR) << "Unable to select communication device!";
      return nullptr;
    }
  }

  if (__builtin_available(android AAUDIO_MIN_API, *)) {
    if (UseAAudioInput()) {
      std::optional<AudioDevice> device =
          GetDeviceForAAudioStream(device_id, AudioDeviceDirection::kInput);
      if (!device.has_value()) {
        return nullptr;
      }
      return new AAudioInputStream(this, params, std::move(device).value());
    }
  }

  // Create a new audio input stream and enable or disable all audio effects
  // given |params.effects()|.
#if BUILDFLAG(USE_OPENSLES)
  return new OpenSLESInputStream(this, params);
#else
  return nullptr;
#endif
}

void AudioManagerAndroid::OnStartAAudioInputStream(AAudioInputStream* stream) {
  // Enable Bluetooth SCO for Bluetooth SCO input streams when per-stream device
  // selection is enabled.

  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  bool stream_requires_sco =
      UseAAudioPerStreamDeviceSelection() &&
      stream->GetDevice().GetType() == AudioDeviceType::kBluetoothSco;
  if (!stream_requires_sco) {
    return;
  }

  input_streams_requiring_sco_.insert(stream);

  // SCO can safely be re-enabled even if it is already on.
  GetJniDelegate().MaybeSetBluetoothScoState(true);

  // TODO(crbug.com/405955144): Call this in response to an appropriate system
  // broadcast instead, in order to correctly react to SCO state changes caused
  // by other apps.
  DVLOG(1) << "Calling SetUseSco(true) for " << bluetooth_output_streams_.size()
           << " Bluetooth streams";
  for (auto bluetooth_output_stream : bluetooth_output_streams_) {
    bluetooth_output_stream->SetUseSco(true);
  }
}

void AudioManagerAndroid::OnStopAAudioInputStream(AAudioInputStream* stream) {
  // Disable Bluetooth SCO when it is no longer needed by any input streams.

  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Only disable SCO if the last stream requiring it was just stopped
  if (input_streams_requiring_sco_.erase(stream) == 0) {
    return;
  }
  if (!input_streams_requiring_sco_.empty()) {
    return;
  }

  GetJniDelegate().MaybeSetBluetoothScoState(false);

  // TODO(crbug.com/405955144): Call this in response to an appropriate system
  // broadcast instead, in order to correctly react to SCO state changes caused
  // by other apps.
  DVLOG(1) << "Calling SetUseSco(false) for "
           << bluetooth_output_streams_.size() << " Bluetooth streams";
  for (auto bluetooth_output_stream : bluetooth_output_streams_) {
    bluetooth_output_stream->SetUseSco(false);
  }
}

void AudioManagerAndroid::SetMute(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jboolean muted) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerAndroid::DoSetMuteOnAudioThread,
                                base::Unretained(this), muted));
}

void AudioManagerAndroid::SetOutputVolumeOverride(double volume) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerAndroid::DoSetVolumeOnAudioThread,
                                base::Unretained(this), volume));
}

bool AudioManagerAndroid::HasOutputVolumeOverride(double* out_volume) const {
  if (output_volume_override_set_) {
    *out_volume = output_volume_override_;
  }
  return output_volume_override_set_;
}

base::TimeDelta AudioManagerAndroid::GetOutputLatency() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return GetJniDelegate().GetOutputLatency();
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // TODO(crbug.com/417397476): Support non-default devices.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";

  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = GetJniDelegate().GetNativeOutputSampleRate();
  int buffer_size = GetOptimalOutputFrameSize(sample_rate, 2);

  // Use the client's input parameters if they are valid.
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();

    // AudioManager APIs for GetOptimalOutputFrameSize() don't support channel
    // layouts greater than stereo unless low latency audio is supported.
    if (input_params.channels() <= 2 ||
        GetJniDelegate().IsAudioLowLatencySupported()) {
      channel_layout_config = input_params.channel_layout_config();
    }

    // For high latency playback on supported platforms, pass through the
    // requested buffer size; this provides significant power savings (~25%) and
    // reduces the potential for glitches under load.
    if (input_params.latency_tag() == AudioLatency::Type::kPlayback) {
      buffer_size = input_params.frames_per_buffer();
    } else {
      buffer_size = GetOptimalOutputFrameSize(sample_rate,
                                              channel_layout_config.channels());
    }
  }

  if (base::FeatureList::IsEnabled(kUseAudioManagerMaxChannelLayout)) {
    // Since channel count never changes over the lifetime of an output stream,
    // use the max number of channels supported. This can prevent down-sampling
    // and loss of channel information (e.g. if a stream starts as stereo and
    // changes to 5.1)
    ChannelLayoutConfig max_channel_layout_config = GetLayoutWithMaxChannels();
    if (max_channel_layout_config.channels() >
        channel_layout_config.channels()) {
      channel_layout_config = max_channel_layout_config;
    }
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size) {
    buffer_size = user_buffer_size;
  }

  // Specify hardware capabilities for HDMI audio passthrough
  AudioParameters::HardwareCapabilities hardware_capabilities(
      GetJniDelegate().GetHdmiOutputEncodingFormats(),
      /*require_encapsulation=*/false);

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size,
                         hardware_capabilities);
}

bool AudioManagerAndroid::HasNoAudioInputStreams() {
  return input_stream_count() == 0;
}

AudioManagerAndroid::JniDelegate& AudioManagerAndroid::GetJniDelegate() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!jni_delegate_) {
    // Create the JNI delegate on the audio thread; prepare the list of audio
    // devices and register receivers for device notifications.
    jni_delegate_ = std::make_unique<JniDelegateImpl>(this);
  }
  return *jni_delegate_;
}

int AudioManagerAndroid::GetOptimalOutputFrameSize(int sample_rate,
                                                   int channels) {
  if (GetJniDelegate().IsAudioLowLatencySupported()) {
    return GetJniDelegate().GetAudioLowLatencyOutputFrameSize();
  }

  return std::max(
      kDefaultOutputBufferSize,
      GetJniDelegate().GetMinOutputFrameSize(sample_rate, channels));
}

AudioParameters::Format AudioManagerAndroid::GetHdmiOutputEncodingFormats() {
  // This method is static, so it cannot use the `JniDelegate`.
  JNIEnv* env = AttachCurrentThread();
  return Java_AudioManagerAndroid_getHdmiOutputEncodingFormats(env);
}

void AudioManagerAndroid::DoSetMuteOnAudioThread(bool muted) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  for (auto stream : output_streams_) {
    stream->SetMute(muted);
  }
}

void AudioManagerAndroid::DoSetVolumeOnAudioThread(double volume) {
  output_volume_override_set_ = true;
  output_volume_override_ = volume;

  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  for (auto stream : output_streams_) {
    stream->SetVolume(volume);
  }
}

ChannelLayoutConfig AudioManagerAndroid::GetLayoutWithMaxChannels() {
  int value = GetJniDelegate().GetLayoutWithMaxChannels();
  CHECK_GT(value, 0);
  CHECK_LE(value, CHANNEL_LAYOUT_MAX);
  ChannelLayout channel_layout = static_cast<ChannelLayout>(value);
  int channel_count = ChannelLayoutToChannelCount(channel_layout);
  return ChannelLayoutConfig(channel_layout, channel_count);
}

void AudioManagerAndroid::SetJniDelegateForTesting(
    std::unique_ptr<JniDelegate> jni_delegate) {
  jni_delegate_ = std::move(jni_delegate);
}

}  // namespace media
