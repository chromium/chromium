// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include <memory>
#include <optional>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/android_buildflags.h"
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
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_OPENSLES)
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/AudioManagerAndroid_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
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

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 10;

const int kDefaultInputBufferSize = 1024;
const int kDefaultOutputBufferSize = 2048;

void AddDefaultDevice(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(AudioDeviceName::CreateDefault());
}

// Returns whether the currently connected device is an audio sink.
bool IsAudioSinkConnected() {
  return Java_AudioManagerAndroid_isAudioSinkConnected(
      base::android::AttachCurrentThread());
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
      FROM_HERE, base::BindOnce(base::IgnoreResult(
                                    &AudioManagerAndroid::GetJavaAudioManager),
                                base::Unretained(this)));
}

void AudioManagerAndroid::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();

  // Destroy java android manager here because it can only be accessed on the
  // audio thread.
  if (!j_audio_manager_.is_null()) {
    DVLOG(2) << "Destroying Java part of the audio manager";
    Java_AudioManagerAndroid_close(base::android::AttachCurrentThread(),
                                   j_audio_manager_);
    j_audio_manager_.Reset();
  }
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
  GetDeviceNames(device_names, AudioDeviceDirection::kCommunication);
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

  // Always add default device parameters as first element.
  DCHECK(device_names->empty());
  AddDefaultDevice(device_names);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_device_array;
  switch (direction) {
    case AudioDeviceDirection::kInput:
    case AudioDeviceDirection::kOutput:
      j_device_array = Java_AudioManagerAndroid_getDevices(
          env, GetJavaAudioManager(),
          /* inputs= */ direction == AudioDeviceDirection::kInput);
      break;
    case AudioDeviceDirection::kCommunication:
      j_device_array = Java_AudioManagerAndroid_getCommunicationDevices(
          env, GetJavaAudioManager());
      break;
  }
  if (j_device_array.is_null()) {
    // Most probable reason for a NULL result here is that the process lacks
    // MODIFY_AUDIO_SETTINGS or RECORD_AUDIO permissions.
    return;
  }

  std::vector<std::pair<AudioDeviceId, AudioDevice>> devices;
  for (auto j_device : j_device_array.ReadElements<jobject>()) {
    jint j_device_id = Java_AudioDevice_id(env, j_device);
    ScopedJavaLocalRef<jstring> j_device_name =
        Java_AudioDevice_name(env, j_device);
    jint j_device_type = Java_AudioDevice_type(env, j_device);

    if (direction == AudioDeviceDirection::kInput ||
        direction == AudioDeviceDirection::kOutput) {
      std::optional<AudioDeviceId> device_id =
          AudioDeviceId::NonDefault(j_device_id);
      if (!device_id.has_value()) {
        LOG(WARNING) << "Unexpectedly received device with default ID";
        continue;
      }

      std::optional<AudioDeviceType> device_type =
          IntToAudioDeviceType(j_device_type);
      if (!device_type.has_value()) {
        LOG(WARNING) << "No device type matching integer value: "
                     << j_device_type;
        device_type = AudioDeviceType::kUnknown;
      }

      AudioDevice device(device_id.value(), device_type.value());
      devices.emplace_back(std::move(device_id).value(), std::move(device));
    }

    std::string device_name;
    if (!j_device_name.is_null()) {
      ConvertJavaStringToUTF8(env, j_device_name.obj(), &device_name);
    } else {
      device_name = "Audio device";  // TODO(crbug.com/409028970): Also return
                                     // the device type and provide a localized,
                                     // type-specific fallback string.
    }

    std::string device_id_string = base::NumberToString(j_device_id);
    device_names->emplace_back(std::move(device_name),
                               std::move(device_id_string));
  }

  if (direction == AudioDeviceDirection::kInput) {
    input_devices_ = base::flat_map(devices);
  } else if (direction == AudioDeviceDirection::kOutput) {
    output_devices_ = base::flat_map(devices);
  }

  for (const AudioDeviceName& d : *device_names) {
    DVLOG(1) << "device_name: " << d.device_name;
    DVLOG(1) << "unique_id: " << d.unique_id;
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

  Devices devices;
  switch (direction) {
    case AudioDeviceDirection::kInput:
      devices = input_devices_;
      break;
    case AudioDeviceDirection::kOutput:
      devices = output_devices_;
      break;
    case AudioDeviceDirection::kCommunication:
      NOTREACHED();
  }
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
  JNIEnv* env = AttachCurrentThread();
  constexpr ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  int buffer_size = Java_AudioManagerAndroid_getMinInputFrameSize(
      env, GetNativeOutputSampleRate(),
      ChannelLayoutToChannelCount(channel_layout));
  buffer_size = buffer_size <= 0 ? kDefaultInputBufferSize : buffer_size;
  int effects = AudioParameters::NO_EFFECTS;
  effects |= Java_AudioManagerAndroid_acousticEchoCancelerIsAvailable(env)
                 ? AudioParameters::ECHO_CANCELLER
                 : AudioParameters::NO_EFFECTS;

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size) {
    buffer_size = user_buffer_size;
  }

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::FromLayout<channel_layout>(),
                         GetNativeOutputSampleRate(), buffer_size);
  params.set_effects(effects);
  DVLOG(1) << params.AsHumanReadableString();
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
  force_communication_mode = IsBluetoothScoOn();
#endif
  if (params.effects() != AudioParameters::NO_EFFECTS ||
      force_communication_mode) {
    communication_mode_is_on_ = true;
    SetCommunicationAudioModeOn(true);
  }
  return stream;
}

void AudioManagerAndroid::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_streams_.erase(static_cast<MuteableAudioOutputStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerAndroid::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioManagerBase::ReleaseInputStream(stream);

  // Restore the audio mode which was used before the first communication-
  // mode stream was created.
  if (HasNoAudioInputStreams() && communication_mode_is_on_) {
    communication_mode_is_on_ = false;
    SetCommunicationAudioModeOn(false);
  }
}

AudioOutputStream* AudioManagerAndroid::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (__builtin_available(android AAUDIO_MIN_API, *)) {
    if (UseAAudioOutput()) {
      return new AAudioOutputStream(this, params, AudioDevice::Default(),
                                    AAUDIO_USAGE_MEDIA);
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
      return new AAudioOutputStream(this, params, std::move(device).value(),
                                    usage);
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
    if (!SetCommunicationDevice(device_id)) {
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
  MaybeSetBluetoothScoState(true);
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

  MaybeSetBluetoothScoState(false);
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
  JNIEnv* env = AttachCurrentThread();
  return base::Milliseconds(
      Java_AudioManagerAndroid_getOutputLatency(env, GetJavaAudioManager()));
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DVLOG(1) << __FUNCTION__;
  // TODO(tommi): Support |output_device_id|.
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = GetNativeOutputSampleRate();
  int buffer_size = GetOptimalOutputFrameSize(sample_rate, 2);
  if (input_params.IsValid()) {
    // Use the client's input parameters if they are valid.
    sample_rate = input_params.sample_rate();

    // AudioManager APIs for GetOptimalOutputFrameSize() don't support channel
    // layouts greater than stereo unless low latency audio is supported.
    if (input_params.channels() <= 2 || IsAudioLowLatencySupported()) {
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
    channel_layout_config = GetLayoutWithMaxChannels(channel_layout_config);
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size) {
    buffer_size = user_buffer_size;
  }

  // Check if device supports additional audio encodings.
  if (IsAudioSinkConnected()) {
    return GetAudioFormatsSupportedBySinkDevice(
        output_device_id, channel_layout_config, sample_rate, buffer_size);
  }
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size);
}

bool AudioManagerAndroid::HasNoAudioInputStreams() {
  return input_stream_count() == 0;
}

const JavaRef<jobject>& AudioManagerAndroid::GetJavaAudioManager() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (j_audio_manager_.is_null()) {
    // Create the Android audio manager on the audio thread.
    DVLOG(2) << "Creating Java part of the audio manager";
    j_audio_manager_.Reset(Java_AudioManagerAndroid_createAudioManagerAndroid(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(this)));

    // Prepare the list of audio devices and register receivers for device
    // notifications.
    Java_AudioManagerAndroid_init(base::android::AttachCurrentThread(),
                                  j_audio_manager_);
  }
  return j_audio_manager_;
}

void AudioManagerAndroid::SetCommunicationAudioModeOn(bool on) {
  DVLOG(1) << __FUNCTION__ << ": " << on;
  Java_AudioManagerAndroid_setCommunicationAudioModeOn(
      base::android::AttachCurrentThread(), GetJavaAudioManager(), on);
}

bool AudioManagerAndroid::SetCommunicationDevice(const std::string& device_id) {
  DVLOG(1) << __FUNCTION__ << ": " << device_id;
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Send the unique device ID to the Java audio manager and make the
  // device switch. Provide an empty string to the Java audio manager
  // if the default device is selected.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_device_id = ConvertUTF8ToJavaString(
      env, device_id == AudioDeviceDescription::kDefaultDeviceId ? std::string()
                                                                 : device_id);
  return Java_AudioManagerAndroid_setCommunicationDevice(
      env, GetJavaAudioManager(), j_device_id);
}

bool AudioManagerAndroid::IsBluetoothScoOn() {
  return Java_AudioManagerAndroid_isBluetoothScoOn(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

void AudioManagerAndroid::MaybeSetBluetoothScoState(bool state) {
  return Java_AudioManagerAndroid_maybeSetBluetoothScoState(
      base::android::AttachCurrentThread(), GetJavaAudioManager(), state);
}

int AudioManagerAndroid::GetNativeOutputSampleRate() {
  return Java_AudioManagerAndroid_getNativeOutputSampleRate(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

bool AudioManagerAndroid::IsAudioLowLatencySupported() {
  return Java_AudioManagerAndroid_isAudioLowLatencySupported(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

int AudioManagerAndroid::GetAudioLowLatencyOutputFrameSize() {
  return Java_AudioManagerAndroid_getAudioLowLatencyOutputFrameSize(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

int AudioManagerAndroid::GetOptimalOutputFrameSize(int sample_rate,
                                                   int channels) {
  if (IsAudioLowLatencySupported()) {
    return GetAudioLowLatencyOutputFrameSize();
  }

  return std::max(
      kDefaultOutputBufferSize,
      Java_AudioManagerAndroid_getMinOutputFrameSize(
          base::android::AttachCurrentThread(), sample_rate, channels));
}

// Returns a bit mask of AudioParameters::Format enum values sink device
// supports.
int AudioManagerAndroid::GetSinkAudioEncodingFormats() {
  JNIEnv* env = AttachCurrentThread();
  return Java_AudioManagerAndroid_getAudioEncodingFormatsSupported(env);
}

// Returns encoding bitstream formats supported by Sink device. Returns
// AudioParameters structure.
AudioParameters AudioManagerAndroid::GetAudioFormatsSupportedBySinkDevice(
    const std::string& output_device_id,
    const ChannelLayoutConfig& channel_layout_config,
    int sample_rate,
    int buffer_size) {
  int formats = GetSinkAudioEncodingFormats();
  DVLOG(1) << __func__ << ": IsAudioSinkConnected()==true, output_device_id="
           << output_device_id << ", Supported Encodings=" << formats;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
      sample_rate, buffer_size,
      AudioParameters::HardwareCapabilities(formats,
                                            /*require_encapsulation=*/false));
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

ChannelLayoutConfig AudioManagerAndroid::GetLayoutWithMaxChannels(
    ChannelLayoutConfig layout_configuration) {
  JNIEnv* env = AttachCurrentThread();
  int value = Java_AudioManagerAndroid_getLayoutWithMaxChannels(
      env, GetJavaAudioManager());
  CHECK_GT(value, 0);
  CHECK_LE(value, CHANNEL_LAYOUT_MAX);
  ChannelLayout channel_layout = static_cast<ChannelLayout>(value);
  int number_channels = ChannelLayoutToChannelCount(channel_layout);
  if (number_channels > layout_configuration.channels()) {
    return ChannelLayoutConfig(channel_layout, number_channels);
  }

  return layout_configuration;
}

}  // namespace media
