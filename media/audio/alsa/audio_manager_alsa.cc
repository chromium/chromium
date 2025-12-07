// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/alsa/audio_manager_alsa.h"

#include <stddef.h>

#include <algorithm>
#include <array>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_view_util.h"
#include "media/audio/alsa/alsa_input.h"
#include "media/audio/alsa/alsa_output.h"
#include "media/audio/alsa/alsa_wrapper.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_output_dispatcher.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
static const int kDefaultSampleRate = 48000;

// Since "default", "pulse" and "dmix" devices are virtual devices mapped to
// real devices, we remove them from the list to avoiding duplicate counting.
// In addition, note that we support no more than 2 channels for recording,
// hence surround devices are not stored in the list.
constexpr auto kInvalidAudioInputDevices = std::to_array<std::string_view>({
    "default",
    "dmix",
    "null",
    "pulse",
    "surround",
});

AudioManagerAlsa::AudioManagerAlsa(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      wrapper_(std::make_unique<AlsaWrapper>()) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerAlsa::~AudioManagerAlsa() = default;

bool AudioManagerAlsa::HasAudioOutputDevices() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAlsaOutputDevice) ||
         HasAnyAlsaAudioDevice(kStreamPlayback);
}

bool AudioManagerAlsa::HasAudioInputDevices() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAlsaInputDevice) ||
         HasAnyAlsaAudioDevice(kStreamCapture);
}

void AudioManagerAlsa::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAlsaAudioDevices(kStreamCapture, device_names);
  AddAlsaDeviceFromSwitch(switches::kAlsaInputDevice, device_names);
}

void AudioManagerAlsa::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAlsaAudioDevices(kStreamPlayback, device_names);
  AddAlsaDeviceFromSwitch(switches::kAlsaOutputDevice, device_names);
}

AudioParameters AudioManagerAlsa::GetInputStreamParameters(
    const std::string& device_id) {
  static const int kDefaultInputBufferSize = 1024;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
                         kDefaultInputBufferSize);
}

const std::string_view AudioManagerAlsa::GetName() {
  return "ALSA";
}

void AudioManagerAlsa::GetAlsaAudioDevices(StreamType type,
                                           AudioDeviceNames* device_names) {
  // Constants specified by the ALSA API for device hints.
  static const char kPcmInterfaceName[] = "pcm";
  int card = -1;

  // Loop through the physical sound cards to get ALSA device hints.
  while (!wrapper_->CardNext(&card) && card >= 0) {
    void** hints = nullptr;
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      GetAlsaDevicesInfo(type, hints, device_names);

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
    } else {
      DLOG(WARNING) << "GetAlsaAudioDevices: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }
}

void AudioManagerAlsa::GetAlsaDevicesInfo(AudioManagerAlsa::StreamType type,
                                          void** hints,
                                          AudioDeviceNames* device_names) {
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";
  static const char kDescriptionHintName[] = "DESC";

  const std::string_view unwanted_device_type =
      UnwantedDeviceTypeWhenEnumerating(type);

  // SAFETY: the ALSA API guarantees that `hint_iter` will dereference to
  // nullptr as the last element before it goes out of bounds.
  for (void** hint_iter = hints; *hint_iter != nullptr;
       UNSAFE_BUFFERS(++hint_iter)) {
    // Only examine devices of the right type.  Valid values are
    // "Input", "Output", and NULL which means both input and output.
    auto io = wrapper_->DeviceNameGetHint(*hint_iter, kIoHintName);
    if (base::as_string_view(io) == unwanted_device_type) {
      continue;
    }

    // Found a device, prepend the default device since we always want
    // it to be on the top of the list for all platforms. And there is
    // no duplicate counting here since it is only done if the list is
    // still empty.  Note, pulse has exclusively opened the default
    // device, so we must open the device via the "default" moniker.
    if (device_names->empty())
      device_names->push_front(AudioDeviceName::CreateDefault());

    // Get the unique device name for the device.
    auto unique_device_name =
        wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName);

    // Find out if the device is available.
    if (IsAlsaDeviceAvailable(type, unique_device_name.data())) {
      // Get the description for the device.
      auto desc = wrapper_->DeviceNameGetHint(*hint_iter, kDescriptionHintName);

      AudioDeviceName name;
      name.unique_id = base::as_string_view(unique_device_name);
      if (!desc.empty()) {
        // Use the more user friendly description as name.
        // Replace '\n' with '-'.
        std::ranges::replace(desc, '\n', '-');
        name.device_name = base::as_string_view(desc);
      } else {
        // Virtual devices don't necessarily have descriptions.
        // Use their names instead.
        name.device_name = base::as_string_view(unique_device_name);
      }

      // Store the device information.
      device_names->push_back(name);
    }
  }
}

// static
bool AudioManagerAlsa::IsAlsaDeviceAvailable(AudioManagerAlsa::StreamType type,
                                             std::string_view device_name) {
  if (device_name.empty()) {
    return false;
  }

  // We do prefix matches on the device name to see whether to include
  // it or not.
  if (type == kStreamCapture) {
    // Check if the device is in the list of invalid devices.
    for (const auto kInvalidAudioInputDevice : kInvalidAudioInputDevices) {
      if (device_name.starts_with(kInvalidAudioInputDevice)) {
        return false;
      }
    }
    return true;
  }

  DCHECK_EQ(kStreamPlayback, type);
  // We prefer the device type that maps straight to hardware but
  // goes through software conversion if needed (e.g. incompatible
  // sample rate).
  // TODO(joi): Should we prefer "hw" instead?
  static constexpr std::string_view kDeviceTypeDesired = "plughw";
  return device_name.starts_with(kDeviceTypeDesired);
}

// static
void AudioManagerAlsa::AddAlsaDeviceFromSwitch(const char* switch_name,
                                               AudioDeviceNames* device_names) {
  // If an ALSA device is specified via the given switch, but the device list
  // does not contain the specified device, append it to the list.
  // GetAlsaAudioDevices only returns hardware ALSA devices, so this logic
  // ensures that if a virtual ALSA device is specified via switch, it is
  // included in the list of audio devices.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    // If the device list is empty, prepend the default device since we always
    // want it to be on the top of the list for all platforms.
    if (device_names->empty()) {
      device_names->push_front(AudioDeviceName::CreateDefault());
    }
    std::string switch_device_name =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switch_name);
    // Only append the specified device if it is not already present on the
    // list.
    if (!base::Contains(
            *device_names, switch_device_name,
            [](const auto& device_name) { return device_name.unique_id; })) {
      AudioDeviceName name;
      name.unique_id = switch_device_name;
      name.device_name = switch_device_name;
      device_names->push_back(name);
    }
  }
}

// static
std::string_view AudioManagerAlsa::UnwantedDeviceTypeWhenEnumerating(
    AudioManagerAlsa::StreamType wanted_type) {
  return wanted_type == kStreamPlayback ? "Input" : "Output";
}

bool AudioManagerAlsa::HasAnyAlsaAudioDevice(
    AudioManagerAlsa::StreamType stream) {
  static const char kPcmInterfaceName[] = "pcm";
  static const char kIoHintName[] = "IOID";
  void** hints = nullptr;
  bool has_device = false;
  int card = -1;

  // Loop through the sound cards.
  // Don't use snd_device_name_hint(-1,..) since there is an access violation
  // inside this ALSA API with libasound.so.2.0.0.
  while (!wrapper_->CardNext(&card) && (card >= 0) && !has_device) {
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      const std::string_view unwanted_type =
          UnwantedDeviceTypeWhenEnumerating(stream);

      // SAFETY: the ALSA API guarantees that `hint_iter` will dereference to
      // nullptr as the last element before it goes out of bounds.
      for (void** hint_iter = hints; *hint_iter != nullptr;
           UNSAFE_BUFFERS(++hint_iter)) {
        // Only examine devices that are |stream| capable.  Valid values are
        // "Input", "Output", and NULL which means both input and output.
        auto io = wrapper_->DeviceNameGetHint(*hint_iter, kIoHintName);
        if (base::as_string_view(io) == unwanted_type) {
          continue;  // Wrong type, skip the device.
        }

        // Found an input device.
        has_device = true;
        break;
      }

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
      hints = nullptr;
    } else {
      DLOG(WARNING) << "HasAnyAudioDevice: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }

  return has_device;
}

AudioOutputStream* AudioManagerAlsa::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerAlsa::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerAlsa::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerAlsa::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerAlsa::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  static const int kDefaultOutputBufferSize = 2048;
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  if (input_params.IsValid()) {
    // Some clients, such as WebRTC, have a more limited use case and work
    // acceptably with a smaller buffer size.  The check below allows clients
    // which want to try a smaller buffer size on Linux to do so.
    // TODO(dalecurtis): This should include bits per channel and channel layout
    // eventually.
    sample_rate = input_params.sample_rate();
    channel_layout_config = input_params.channel_layout_config();
    buffer_size = std::min(input_params.frames_per_buffer(), buffer_size);
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size);
}

AudioOutputStream* AudioManagerAlsa::MakeOutputStream(
    const AudioParameters& params) {
  std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlsaOutputDevice)) {
    device_name = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaOutputDevice);
  }
  return new AlsaPcmOutputStream(device_name, params, wrapper_.get(), this);
}

AudioInputStream* AudioManagerAlsa::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  std::string device_name =
      (device_id == AudioDeviceDescription::kDefaultDeviceId)
          ? AlsaPcmInputStream::kAutoSelectDevice
          : device_id;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlsaInputDevice)) {
    device_name = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaInputDevice);
  }

  return new AlsaPcmInputStream(this, device_name, params, wrapper_.get());
}

}  // namespace media
