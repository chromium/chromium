// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/core_audio_util_win.h"

#include <objbase.h>

#include <comdef.h>
#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stddef.h>
#include <stdint.h>

#include <bitset>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"

using Microsoft::WRL::ComPtr;
using base::win::ScopedCoMem;
using base::win::ScopedHandle;

namespace media {

// See header file for documentation.
// {BE39AF4F-087C-423F-9303-234EC1E5B8EE}
const GUID kCommunicationsSessionId = {
  0xbe39af4f, 0x87c, 0x423f, { 0x93, 0x3, 0x23, 0x4e, 0xc1, 0xe5, 0xb8, 0xee }
};

namespace {

constexpr uint32_t KSAUDIO_SPEAKER_UNSUPPORTED = 0xFFFFFFFF;

REFERENCE_TIME BufferSizeInFramesToTimeDelta(uint32_t frames,
                                             DWORD bytes_per_sec,
                                             WORD bytes_per_frame) {
  constexpr double ref_time_ns = 10000000;  // 10ms
  return (REFERENCE_TIME)((double)(frames)*ref_time_ns /
                              ((double)(bytes_per_sec / bytes_per_frame)) +
                          0.5f);
}

// Return the requested offload buffer time in 100ns units.
double GetOffloadBufferTimeIn100Ns() {
  if (base::FeatureList::IsEnabled(kAudioOffload)) {
    return media::kAudioOffloadBufferTimeMs.Get() * 10000;
  }

  return 500000;  // 50ms
}

// TODO(henrika): add mapping for all types in the ChannelLayout enumerator.
ChannelConfig ChannelLayoutToChannelConfig(ChannelLayout layout) {
  switch (layout) {
    case CHANNEL_LAYOUT_DISCRETE:
      DVLOG(2) << "CHANNEL_LAYOUT_DISCRETE=>KSAUDIO_SPEAKER_DIRECTOUT";
      return KSAUDIO_SPEAKER_DIRECTOUT;
    case CHANNEL_LAYOUT_MONO:
      DVLOG(2) << "CHANNEL_LAYOUT_MONO=>KSAUDIO_SPEAKER_MONO";
      return KSAUDIO_SPEAKER_MONO;
    case CHANNEL_LAYOUT_STEREO:
      DVLOG(2) << "CHANNEL_LAYOUT_STEREO=>KSAUDIO_SPEAKER_STEREO";
      return KSAUDIO_SPEAKER_STEREO;
    case CHANNEL_LAYOUT_2POINT1:
      DVLOG(2) << "CHANNEL_LAYOUT_2POINT1=>KSAUDIO_SPEAKER_2POINT1";
      return KSAUDIO_SPEAKER_2POINT1;
    case CHANNEL_LAYOUT_SURROUND:
      DVLOG(2) << "CHANNEL_LAYOUT_SURROUND=>KSAUDIO_SPEAKER_3POINT0";
      return KSAUDIO_SPEAKER_3POINT0;
    case CHANNEL_LAYOUT_3_1:
      DVLOG(2) << "CHANNEL_LAYOUT_3_1=>KSAUDIO_SPEAKER_3POINT1";
      return KSAUDIO_SPEAKER_3POINT1;
    case CHANNEL_LAYOUT_QUAD:
      DVLOG(2) << "CHANNEL_LAYOUT_QUAD=>KSAUDIO_SPEAKER_QUAD";
      return KSAUDIO_SPEAKER_QUAD;
    case CHANNEL_LAYOUT_4_0:
      DVLOG(2) << "CHANNEL_LAYOUT_4_0=>KSAUDIO_SPEAKER_SURROUND";
      return KSAUDIO_SPEAKER_SURROUND;
    case CHANNEL_LAYOUT_5_0:
      DVLOG(2) << "CHANNEL_LAYOUT_5_0=>KSAUDIO_SPEAKER_5POINT0";
      return KSAUDIO_SPEAKER_5POINT0;
    case CHANNEL_LAYOUT_5_1_BACK:
      DVLOG(2) << "CHANNEL_LAYOUT_5_1_BACK=>KSAUDIO_SPEAKER_5POINT1";
      return KSAUDIO_SPEAKER_5POINT1;
    case CHANNEL_LAYOUT_5_1:
      DVLOG(2) << "CHANNEL_LAYOUT_5_1=>KSAUDIO_SPEAKER_5POINT1_SURROUND";
      return KSAUDIO_SPEAKER_5POINT1_SURROUND;
    case CHANNEL_LAYOUT_7_0:
      DVLOG(2) << "CHANNEL_LAYOUT_7_0=>KSAUDIO_SPEAKER_7POINT0";
      return KSAUDIO_SPEAKER_7POINT0;
    case CHANNEL_LAYOUT_7_1_WIDE_BACK:
      DVLOG(2) << "CHANNEL_LAYOUT_7_1_WIDE_BACK=>KSAUDIO_SPEAKER_7POINT1";
      return KSAUDIO_SPEAKER_7POINT1;
    case CHANNEL_LAYOUT_7_1:
      DVLOG(2) << "CHANNEL_LAYOUT_7_1=>KSAUDIO_SPEAKER_7POINT1_SURROUND";
      return KSAUDIO_SPEAKER_7POINT1_SURROUND;
    default:
      DVLOG(2) << "Unsupported channel layout: " << layout;
      return KSAUDIO_SPEAKER_UNSUPPORTED;
  }
}

// Converts the most common format tags defined in mmreg.h into string
// equivalents. Mainly intended for log messages.
const char* WaveFormatTagToString(WORD format_tag) {
  switch (format_tag) {
    case WAVE_FORMAT_UNKNOWN:
      return "WAVE_FORMAT_UNKNOWN";
    case WAVE_FORMAT_PCM:
      return "WAVE_FORMAT_PCM";
    case WAVE_FORMAT_IEEE_FLOAT:
      return "WAVE_FORMAT_IEEE_FLOAT";
    case WAVE_FORMAT_EXTENSIBLE:
      return "WAVE_FORMAT_EXTENSIBLE";
    default:
      return "UNKNOWN";
  }
}

// Converts from channel mask to list of included channels.
// Each audio data format contains channels for one or more of the positions
// listed below. The number of channels simply equals the number of nonzero
// flag bits in the |channel_mask|. The relative positions of the channels
// within each block of audio data always follow the same relative ordering
// as the flag bits in the table below. For example, if |channel_mask| contains
// the value 0x00000033, the format defines four audio channels that are
// assigned for playback to the front-left, front-right, back-left,
// and back-right speakers, respectively. The channel data should be interleaved
// in that order within each block.
std::string ChannelMaskToString(DWORD channel_mask) {
  std::string ss;
  if (channel_mask == KSAUDIO_SPEAKER_DIRECTOUT)
    // A very rare channel mask where speaker orientation is "hard coded".
    // In direct-out mode, the audio device renders the first channel to the
    // first output connector on the device, the second channel to the second
    // output on the device, and so on.
    ss += "DIRECT_OUT";
  else {
    if (channel_mask & SPEAKER_FRONT_LEFT)
      ss += "FRONT_LEFT | ";
    if (channel_mask & SPEAKER_FRONT_RIGHT)
      ss += "FRONT_RIGHT | ";
    if (channel_mask & SPEAKER_FRONT_CENTER)
      ss += "FRONT_CENTER | ";
    if (channel_mask & SPEAKER_LOW_FREQUENCY)
      ss += "LOW_FREQUENCY | ";
    if (channel_mask & SPEAKER_BACK_LEFT)
      ss += "BACK_LEFT | ";
    if (channel_mask & SPEAKER_BACK_RIGHT)
      ss += "BACK_RIGHT | ";
    if (channel_mask & SPEAKER_FRONT_LEFT_OF_CENTER)
      ss += "FRONT_LEFT_OF_CENTER | ";
    if (channel_mask & SPEAKER_FRONT_RIGHT_OF_CENTER)
      ss += "RIGHT_OF_CENTER | ";
    if (channel_mask & SPEAKER_BACK_CENTER)
      ss += "BACK_CENTER | ";
    if (channel_mask & SPEAKER_SIDE_LEFT)
      ss += "SIDE_LEFT | ";
    if (channel_mask & SPEAKER_SIDE_RIGHT)
      ss += "SIDE_RIGHT | ";
    if (channel_mask & SPEAKER_TOP_CENTER)
      ss += "TOP_CENTER | ";
    if (channel_mask & SPEAKER_TOP_FRONT_LEFT)
      ss += "TOP_FRONT_LEFT | ";
    if (channel_mask & SPEAKER_TOP_FRONT_CENTER)
      ss += "TOP_FRONT_CENTER | ";
    if (channel_mask & SPEAKER_TOP_FRONT_RIGHT)
      ss += "TOP_FRONT_RIGHT | ";
    if (channel_mask & SPEAKER_TOP_BACK_LEFT)
      ss += "TOP_BACK_LEFT | ";
    if (channel_mask & SPEAKER_TOP_BACK_CENTER)
      ss += "TOP_BACK_CENTER | ";
    if (channel_mask & SPEAKER_TOP_BACK_RIGHT)
      ss += "TOP_BACK_RIGHT | ";

    if (!ss.empty()) {
      // Delete last appended " | " substring.
      ss.erase(ss.end() - 3, ss.end());
    }
  }

  // Add number of utilized channels, e.g. "(2)" but exclude this part for
  // direct output mode since the number of ones in the channel mask does not
  // reflect the number of channels for this case.
  if (channel_mask != KSAUDIO_SPEAKER_DIRECTOUT) {
    std::bitset<8 * sizeof(DWORD)> mask(channel_mask);
    ss += " (";
    ss += std::to_string(mask.count());
    ss += ")";
  }
  return ss;
}

// Converts a channel count into a channel configuration.
ChannelConfig GuessChannelConfig(WORD channels) {
  switch (channels) {
    case 0:
      DVLOG(2) << "KSAUDIO_SPEAKER_DIRECTOUT";
      return KSAUDIO_SPEAKER_DIRECTOUT;
    case 1:
      DVLOG(2) << "KSAUDIO_SPEAKER_MONO";
      return KSAUDIO_SPEAKER_MONO;
    case 2:
      DVLOG(2) << "KSAUDIO_SPEAKER_STEREO";
      return KSAUDIO_SPEAKER_STEREO;
    case 3:
      DVLOG(2) << "KSAUDIO_SPEAKER_2POINT1";
      return KSAUDIO_SPEAKER_2POINT1;
    case 4:
      DVLOG(2) << "KSAUDIO_SPEAKER_QUAD";
      return KSAUDIO_SPEAKER_QUAD;
    case 5:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT0";
      return KSAUDIO_SPEAKER_5POINT0;
    case 6:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1";
      return KSAUDIO_SPEAKER_5POINT1;
    case 7:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT0";
      return KSAUDIO_SPEAKER_7POINT0;
    case 8:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1";
      return KSAUDIO_SPEAKER_7POINT1;
    default:
      DVLOG(1) << "Unsupported channel count: " << channels;
  }
  return KSAUDIO_SPEAKER_STEREO;
}

bool IAudioClient3IsSupported() {
  return base::FeatureList::IsEnabled(features::kAllowIAudioClient3);
}

std::string GetDeviceID(IMMDevice* device) {
  ScopedCoMem<WCHAR> device_id_com;
  std::string device_id;
  if (SUCCEEDED(device->GetId(&device_id_com)))
    base::WideToUTF8(device_id_com, wcslen(device_id_com), &device_id);
  return device_id;
}

bool IsDeviceActive(IMMDevice* device) {
  DWORD state = DEVICE_STATE_DISABLED;
  return SUCCEEDED(device->GetState(&state)) && (state & DEVICE_STATE_ACTIVE);
}

HRESULT GetDeviceFriendlyNameInternal(IMMDevice* device,
                                      std::string* friendly_name) {
  // Retrieve user-friendly name of endpoint device.
  // Example: "Microphone (Realtek High Definition Audio)".
  ComPtr<IPropertyStore> properties;
  HRESULT hr = device->OpenPropertyStore(STGM_READ, &properties);
  if (FAILED(hr))
    return hr;

  base::win::ScopedPropVariant friendly_name_pv;
  hr = properties->GetValue(PKEY_Device_FriendlyName,
                            friendly_name_pv.Receive());
  if (FAILED(hr))
    return hr;

  if (friendly_name_pv.get().vt == VT_LPWSTR &&
      friendly_name_pv.get().pwszVal) {
    base::WideToUTF8(friendly_name_pv.get().pwszVal,
                     wcslen(friendly_name_pv.get().pwszVal), friendly_name);
  }

  return hr;
}

ComPtr<IMMDeviceEnumerator> CreateDeviceEnumeratorInternal(
    bool allow_reinitialize) {
  ComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&device_enumerator));
  if (hr == CO_E_NOTINITIALIZED && allow_reinitialize) {
    LOG(ERROR) << "CoCreateInstance fails with CO_E_NOTINITIALIZED";
    // Buggy third-party DLLs can uninitialize COM out from under us.  Attempt
    // to re-initialize it.  See http://crbug.com/378465 for more details.
    CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    return CreateDeviceEnumeratorInternal(false);
  }
  return device_enumerator;
}

ChannelLayout GetChannelLayout(
    const CoreAudioUtil::WaveFormatWrapper mix_format) {
  if (!mix_format.IsExtensible()) {
    DVLOG(1) << "Format does not contain any channel mask."
             << " Guessing layout by channel count: " << std::dec
             << mix_format->nChannels;
    return GuessChannelLayout(mix_format->nChannels);
  }

  // Get the integer mask which corresponds to the channel layout the
  // audio engine uses for its internal processing/mixing of shared-mode
  // streams. This mask indicates which channels are present in the multi-
  // channel stream. The least significant bit corresponds with the Front
  // Left speaker, the next least significant bit corresponds to the Front
  // Right speaker, and so on, continuing in the order defined in KsMedia.h.
  // See
  // http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083.aspx
  // for more details.
  ChannelConfig channel_config = mix_format.GetExtensible()->dwChannelMask;

  // Convert Microsoft's channel configuration to generic ChannelLayout.
  ChannelLayout channel_layout = ChannelConfigToChannelLayout(channel_config);

  // Some devices don't appear to set a valid channel layout, so guess based
  // on the number of channels.  See http://crbug.com/311906.
  if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
    DVLOG(1) << "Unsupported channel config: " << std::hex << channel_config
             << ".  Guessing layout by channel count: " << std::dec
             << mix_format->nChannels;
    channel_layout = GuessChannelLayout(mix_format->nChannels);
  }
  DVLOG(1) << "channel layout: " << ChannelLayoutToString(channel_layout);

  return channel_layout;
}

bool IsSupportedInternal() {
  // It is possible to force usage of WaveXxx APIs by using a command line
  // flag.
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kForceWaveAudio)) {
    DVLOG(1) << "Forcing usage of Windows WaveXxx APIs";
    return false;
  }

  // Verify that it is possible to a create the IMMDeviceEnumerator interface.
  ComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumeratorInternal(false);
  if (!device_enumerator) {
    LOG(ERROR)
        << "Failed to create Core Audio device enumerator on thread with ID "
        << GetCurrentThreadId();
    return false;
  }

  return true;
}

// Retrieve an audio device specified by |device_id| or a default device
// specified by data-flow direction and role if |device_id| is default.
ComPtr<IMMDevice> CreateDeviceInternal(const std::string& device_id,
                                       EDataFlow data_flow,
                                       ERole role) {
  ComPtr<IMMDevice> endpoint_device;
  // In loopback mode, a client of WASAPI can capture the audio stream that
  // is being played by a rendering endpoint device.
  // See https://crbug.com/956526 for why we use both a DCHECK and then deal
  // with the error here and below.
  DCHECK(!(AudioDeviceDescription::IsLoopbackDevice(device_id) &&
           data_flow != eCapture));
  if (AudioDeviceDescription::IsLoopbackDevice(device_id) &&
      data_flow != eCapture) {
    LOG(WARNING) << "Loopback device must be an input device";
    return endpoint_device;
  }

  // Usage of AudioDeviceDescription::kCommunicationsDeviceId as |device_id|
  // is not allowed. Instead, set |device_id| to kDefaultDeviceId and select
  // between default device and default communication device by using different
  // |role| values (eConsole or eCommunications).
  DCHECK(!AudioDeviceDescription::IsCommunicationsDevice(device_id));
  if (AudioDeviceDescription::IsCommunicationsDevice(device_id)) {
    LOG(WARNING) << "Invalid device identifier";
    return endpoint_device;
  }

  // Create the IMMDeviceEnumerator interface.
  ComPtr<IMMDeviceEnumerator> device_enum(CreateDeviceEnumeratorInternal(true));
  if (!device_enum.Get())
    return endpoint_device;

  HRESULT hr;
  if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
    hr =
        device_enum->GetDefaultAudioEndpoint(data_flow, role, &endpoint_device);
  } else if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    // To open a stream in loopback mode, the client must obtain an IMMDevice
    // interface for the *rendering* endpoint device.
    hr = device_enum->GetDefaultAudioEndpoint(eRender, role, &endpoint_device);
  } else {
    hr = device_enum->GetDevice(base::UTF8ToWide(device_id).c_str(),
                                &endpoint_device);
  }
  DVLOG_IF(1, FAILED(hr)) << "Create Device failed: " << std::hex << hr;

  // Verify that the audio endpoint device is active, i.e., that the audio
  // adapter that connects to the endpoint device is present and enabled.
  if (SUCCEEDED(hr) && !IsDeviceActive(endpoint_device.Get())) {
    DVLOG(1) << "Selected endpoint device is not active";
    endpoint_device.Reset();
    hr = E_FAIL;
  }

  return endpoint_device;
}

// Decide on data_flow and role based on |device_id|, and return the
// corresponding audio device.
ComPtr<IMMDevice> CreateDeviceByID(const std::string& device_id,
                                   bool is_output_device) {
  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    DCHECK(!is_output_device);
    return CreateDeviceInternal(AudioDeviceDescription::kDefaultDeviceId,
                                eRender, eConsole);
  }

  EDataFlow data_flow = is_output_device ? eRender : eCapture;
  if (device_id == AudioDeviceDescription::kCommunicationsDeviceId)
    return CreateDeviceInternal(AudioDeviceDescription::kDefaultDeviceId,
                                data_flow, eCommunications);

  // If AudioDeviceDescription::IsDefaultDevice(device_id), a default device
  // will be created
  return CreateDeviceInternal(device_id, data_flow, eConsole);
}

// Creates and activates an IAudioClient COM object given the selected
// endpoint device.
ComPtr<IAudioClient> CreateClientInternal(IMMDevice* audio_device) {
  if (!audio_device)
    return ComPtr<IAudioClient>();

  ComPtr<IAudioClient> audio_client;
  HRESULT hr = audio_device->Activate(
      __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, &audio_client);
  DVLOG_IF(1, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  return audio_client;
}

// Creates and activates an IAudioClient3 COM object given the selected
// endpoint device.
ComPtr<IAudioClient3> CreateClientInternal3(IMMDevice* audio_device) {
  if (!audio_device)
    return ComPtr<IAudioClient3>();

  ComPtr<IAudioClient3> audio_client;
  HRESULT hr = audio_device->Activate(
      __uuidof(IAudioClient3), CLSCTX_INPROC_SERVER, NULL, &audio_client);
  DVLOG_IF(1, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  return audio_client;
}

HRESULT GetPreferredAudioParametersInternal(IAudioClient* client,
                                            bool is_output_device,
                                            AudioParameters* params,
                                            bool is_offload_stream) {
  WAVEFORMATEXTENSIBLE mix_format;
  HRESULT hr = CoreAudioUtil::GetSharedModeMixFormat(client, &mix_format);
  if (FAILED(hr))
    return hr;
  CoreAudioUtil::WaveFormatWrapper format(&mix_format);

  int min_frames_per_buffer = 0;
  int max_frames_per_buffer = 0;
  int default_frames_per_buffer = 0;
  int frames_per_buffer = 0;

  const bool supports_iac3 = IAudioClient3IsSupported();

  const int sample_rate = format->nSamplesPerSec;
  if (is_offload_stream) {
    frames_per_buffer = AudioTimestampHelper::TimeToFrames(
        CoreAudioUtil::ReferenceTimeToTimeDelta(GetOffloadBufferTimeIn100Ns()),
        sample_rate);
    ComPtr<IAudioClient> audio_client(client);
    ComPtr<IAudioClient2> audio_client_2;
    hr = audio_client.As(&audio_client_2);
    if (SUCCEEDED(hr)) {
      REFERENCE_TIME min_buffer_duration = 0;
      REFERENCE_TIME max_buffer_duration = 0;
      audio_client_2->GetBufferSizeLimits(
          &mix_format.Format, true, &min_buffer_duration, &max_buffer_duration);

      min_frames_per_buffer = AudioTimestampHelper::TimeToFrames(
          CoreAudioUtil::ReferenceTimeToTimeDelta(min_buffer_duration),
          sample_rate);
      max_frames_per_buffer = AudioTimestampHelper::TimeToFrames(
          CoreAudioUtil::ReferenceTimeToTimeDelta(max_buffer_duration),
          sample_rate);
    }
  } else {
    if (supports_iac3) {
      // Try to obtain an IAudioClient3 interface from the IAudioClient object.
      // Use ComPtr::As for doing QueryInterface calls on COM objects.
      ComPtr<IAudioClient> audio_client(client);
      ComPtr<IAudioClient3> audio_client_3;
      hr = audio_client.As(&audio_client_3);
      if (SUCCEEDED(hr)) {
        UINT32 default_period_frames = 0;
        UINT32 fundamental_period_frames = 0;
        UINT32 min_period_frames = 0;
        UINT32 max_period_frames = 0;
        hr = audio_client_3->GetSharedModeEnginePeriod(
            format.get(), &default_period_frames, &fundamental_period_frames,
            &min_period_frames, &max_period_frames);

        if (SUCCEEDED(hr)) {
          min_frames_per_buffer = min_period_frames;
          max_frames_per_buffer = max_period_frames;
          default_frames_per_buffer = default_period_frames;
          frames_per_buffer = default_period_frames;
        }
        DVLOG(1) << "IAudioClient3 => min_period_frames: " << min_period_frames;
        DVLOG(1) << "IAudioClient3 => frames_per_buffer: " << frames_per_buffer;
      }
    }

    // If we don't have access to IAudioClient3 or if the call to
    // GetSharedModeEnginePeriod() fails we fall back to GetDevicePeriod().
    if (!supports_iac3 || FAILED(hr)) {
      REFERENCE_TIME default_period = 0;
      hr = CoreAudioUtil::GetDevicePeriod(client, AUDCLNT_SHAREMODE_SHARED,
                                          &default_period);
      if (FAILED(hr)) {
        return hr;
      }

      // We are using the native device period to derive the smallest possible
      // buffer size in shared mode. Note that the actual endpoint buffer will
      // be larger than this size but it will be possible to fill it up in two
      // calls.
      frames_per_buffer = static_cast<int>(
          sample_rate * CoreAudioUtil::ReferenceTimeToTimeDelta(default_period)
                            .InSecondsF() +
          0.5);
      DVLOG(1) << "IAudioClient => frames_per_buffer: " << frames_per_buffer;
    }
  }

  // Retrieve the current channel configuration (e.g. CHANNEL_LAYOUT_STEREO).
  ChannelLayout channel_layout = GetChannelLayout(format);
  int channels = ChannelLayoutToChannelCount(channel_layout);
  if (channel_layout == CHANNEL_LAYOUT_DISCRETE) {
    if (!is_output_device) {
      // Set the number of channels explicitly to two for input devices if
      // the channel layout is discrete to ensure that the parameters are valid
      // and that clients does not have to support multi-channel input cases.
      // Any required down-mixing from N (N > 2) to 2 must be performed by the
      // input stream implementation instead.
      // See crbug.com/868026 for examples where this approach is needed.
      DVLOG(1) << "Forcing number of channels to 2 for CHANNEL_LAYOUT_DISCRETE";
      channels = 2;
    } else {
      // Some output devices return CHANNEL_LAYOUT_DISCRETE. Keep this channel
      // format but update the number of channels with the correct value. The
      // number of channels will be zero otherwise.
      // See crbug.com/957886 for more details.
      DVLOG(1) << "Setting number of channels to " << format->nChannels
               << " for CHANNEL_LAYOUT_DISCRETE";
      channels = format->nChannels;
    }
  }

  AudioParameters audio_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, {channel_layout, channels},
      sample_rate, frames_per_buffer,
      AudioParameters::HardwareCapabilities(
          min_frames_per_buffer, max_frames_per_buffer,
          default_frames_per_buffer, is_offload_stream));

  DVLOG(1) << audio_params.AsHumanReadableString();
  DCHECK(audio_params.IsValid());
  *params = audio_params;

  return hr;
}

}  // namespace

// CoreAudioUtil::WaveFormatWrapper implementation.
WAVEFORMATEXTENSIBLE* CoreAudioUtil::WaveFormatWrapper::GetExtensible() const {
  CHECK(IsExtensible());
  return reinterpret_cast<WAVEFORMATEXTENSIBLE*>(ptr_.get());
}

bool CoreAudioUtil::WaveFormatWrapper::IsExtensible() const {
  return ptr_->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ptr_->cbSize >= 22;
}

bool CoreAudioUtil::WaveFormatWrapper::IsPcm() const {
  return IsExtensible() ? GetExtensible()->SubFormat == KSDATAFORMAT_SUBTYPE_PCM
                        : ptr_->wFormatTag == WAVE_FORMAT_PCM;
}

bool CoreAudioUtil::WaveFormatWrapper::IsFloat() const {
  return IsExtensible()
             ? GetExtensible()->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
             : ptr_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
}

size_t CoreAudioUtil::WaveFormatWrapper::size() const {
  return sizeof(*ptr_) + ptr_->cbSize;
}

bool CoreAudioUtil::IsSupported() {
  static bool g_is_supported = IsSupportedInternal();
  return g_is_supported;
}

std::string CoreAudioUtil::ErrorToString(HRESULT hresult) {
  const _com_error error(hresult);
  // If the HRESULT is within the range 0x80040200 to 0x8004FFFF, the WCode()
  // method returns the HRESULT minus 0x80040200; otherwise, it returns zero.
  return base::StringPrintf("HRESULT: 0x%08lX, WCode: %u, message: \"%s\"",
                            error.Error(), error.WCode(),
                            base::WideToUTF8(error.ErrorMessage()).c_str());
}

std::string CoreAudioUtil::WaveFormatToString(const WaveFormatWrapper format) {
  // Start with the WAVEFORMATEX part.
  std::string wave_format = base::StringPrintf(
      "wFormatTag: %s (0x%X), nChannels: %d, nSamplesPerSec: %lu"
      ", nAvgBytesPerSec: %lu, nBlockAlign: %d, wBitsPerSample: %d, cbSize: %d",
      WaveFormatTagToString(format->wFormatTag), format->wFormatTag,
      format->nChannels, format->nSamplesPerSec, format->nAvgBytesPerSec,
      format->nBlockAlign, format->wBitsPerSample, format->cbSize);
  if (!format.IsExtensible())
    return wave_format;

  // Append the WAVEFORMATEXTENSIBLE part (which we know exists).
  base::StringAppendF(
      &wave_format, " [+] wValidBitsPerSample: %d, dwChannelMask: %s",
      format.GetExtensible()->Samples.wValidBitsPerSample,
      ChannelMaskToString(format.GetExtensible()->dwChannelMask).c_str());
  if (format.IsPcm()) {
    base::StringAppendF(&wave_format, "%s",
                        ", SubFormat: KSDATAFORMAT_SUBTYPE_PCM");
  } else if (format.IsFloat()) {
    base::StringAppendF(&wave_format, "%s",
                        ", SubFormat: KSDATAFORMAT_SUBTYPE_IEEE_FLOAT");
  } else {
    base::StringAppendF(&wave_format, "%s", ", SubFormat: NOT_SUPPORTED");
  }
  return wave_format;
}

base::TimeDelta CoreAudioUtil::ReferenceTimeToTimeDelta(REFERENCE_TIME time) {
  // Each unit of reference time is 100 nanoseconds <=> 0.1 microsecond.
  return base::Microseconds(0.1 * time + 0.5);
}

AUDCLNT_SHAREMODE CoreAudioUtil::GetShareMode() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio))
    return AUDCLNT_SHAREMODE_EXCLUSIVE;
  return AUDCLNT_SHAREMODE_SHARED;
}

int CoreAudioUtil::NumberOfActiveDevices(EDataFlow data_flow) {
  // Create the IMMDeviceEnumerator interface.
  ComPtr<IMMDeviceEnumerator> device_enumerator = CreateDeviceEnumerator();
  if (!device_enumerator.Get())
    return 0;

  // Generate a collection of active (present and not disabled) audio endpoint
  // devices for the specified data-flow direction.
  // This method will succeed even if all devices are disabled.
  ComPtr<IMMDeviceCollection> collection;
  HRESULT hr = device_enumerator->EnumAudioEndpoints(
      data_flow, DEVICE_STATE_ACTIVE, &collection);
  if (FAILED(hr)) {
    LOG(ERROR) << "IMMDeviceCollection::EnumAudioEndpoints: " << std::hex << hr;
    return 0;
  }

  // Retrieve the number of active audio devices for the specified direction
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  DVLOG(2) << ((data_flow == eCapture) ? "[in ] " : "[out] ")
           << "number of devices: " << number_of_active_devices;
  return static_cast<int>(number_of_active_devices);
}

ComPtr<IMMDeviceEnumerator> CoreAudioUtil::CreateDeviceEnumerator() {
  return CreateDeviceEnumeratorInternal(true);
}

std::string CoreAudioUtil::GetDefaultInputDeviceID() {
  ComPtr<IMMDevice> device(CreateDevice(
      AudioDeviceDescription::kDefaultDeviceId, eCapture, eConsole));
  return device.Get() ? GetDeviceID(device.Get()) : std::string();
}

std::string CoreAudioUtil::GetDefaultOutputDeviceID() {
  ComPtr<IMMDevice> device(CreateDevice(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole));
  return device.Get() ? GetDeviceID(device.Get()) : std::string();
}

std::string CoreAudioUtil::GetCommunicationsInputDeviceID() {
  ComPtr<IMMDevice> device(
      CreateDevice(std::string(), eCapture, eCommunications));
  return device.Get() ? GetDeviceID(device.Get()) : std::string();
}

std::string CoreAudioUtil::GetCommunicationsOutputDeviceID() {
  ComPtr<IMMDevice> device(
      CreateDevice(std::string(), eRender, eCommunications));
  return device.Get() ? GetDeviceID(device.Get()) : std::string();
}

HRESULT CoreAudioUtil::GetDeviceName(IMMDevice* device, AudioDeviceName* name) {
  // Retrieve unique name of endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
  AudioDeviceName device_name;
  device_name.unique_id = GetDeviceID(device);
  if (device_name.unique_id.empty())
    return E_FAIL;

  HRESULT hr = GetDeviceFriendlyNameInternal(device, &device_name.device_name);
  if (FAILED(hr))
    return hr;

  *name = device_name;
  DVLOG(2) << "friendly name: " << device_name.device_name;
  DVLOG(2) << "unique id    : " << device_name.unique_id;
  return hr;
}

std::string CoreAudioUtil::GetAudioControllerID(IMMDevice* device,
    IMMDeviceEnumerator* enumerator) {
  // Fetching the controller device id could be as simple as fetching the value
  // of the "{B3F8FA53-0004-438E-9003-51A46E139BFC},2" property in the property
  // store of the |device|, but that key isn't defined in any header and
  // according to MS should not be relied upon.
  // So, instead, we go deeper, look at the device topology and fetch the
  // PKEY_Device_InstanceId of the associated physical audio device.
  ComPtr<IDeviceTopology> topology;
  ComPtr<IConnector> connector;
  ScopedCoMem<WCHAR> filter_id;
  if (FAILED(device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL,
                              &topology)) ||
      // For our purposes checking the first connected device should be enough
      // and if there are cases where there are more than one device connected
      // we're not sure how to handle that anyway. So we pass 0.
      FAILED(topology->GetConnector(0, &connector)) ||
      FAILED(connector->GetDeviceIdConnectedTo(&filter_id))) {
    DLOG(ERROR) << "Failed to get the device identifier of the audio device";
    return std::string();
  }

  // Now look at the properties of the connected device node and fetch the
  // instance id (PKEY_Device_InstanceId) of the device node that uniquely
  // identifies the controller.
  ComPtr<IMMDevice> device_node;
  ComPtr<IPropertyStore> properties;
  base::win::ScopedPropVariant instance_id;
  if (FAILED(enumerator->GetDevice(filter_id, &device_node)) ||
      FAILED(device_node->OpenPropertyStore(STGM_READ, &properties)) ||
      FAILED(properties->GetValue(PKEY_Device_InstanceId,
                                  instance_id.Receive())) ||
      instance_id.get().vt != VT_LPWSTR) {
    DLOG(ERROR) << "Failed to get instance id of the audio device node";
    return std::string();
  }

  std::string controller_id;
  base::WideToUTF8(instance_id.get().pwszVal,
                   wcslen(instance_id.get().pwszVal),
                   &controller_id);

  return controller_id;
}

std::string CoreAudioUtil::GetMatchingOutputDeviceID(
    const std::string& input_device_id) {
  // Special handling for the default communications device.
  // We always treat the configured communications devices, as a pair.
  // If we didn't do that and the user has e.g. configured a mic of a headset
  // as the default comms input device and a different device (not the speakers
  // of the headset) as the default comms output device, then we would otherwise
  // here pick the headset as the matched output device.  That's technically
  // correct, but the user experience would be that any audio played out to
  // the matched device, would get ducked since it's not the default comms
  // device.  So here, we go with the user's configuration.
  if (input_device_id == AudioDeviceDescription::kCommunicationsDeviceId)
    return AudioDeviceDescription::kCommunicationsDeviceId;

  ComPtr<IMMDevice> input_device(
      CreateDevice(input_device_id, eCapture, eConsole));

  if (!input_device.Get())
    return std::string();

  // See if we can get id of the associated controller.
  ComPtr<IMMDeviceEnumerator> enumerator(CreateDeviceEnumerator());
  std::string controller_id(
      GetAudioControllerID(input_device.Get(), enumerator.Get()));
  if (controller_id.empty())
    return std::string();

  // Now enumerate the available (and active) output devices and see if any of
  // them is associated with the same controller.
  ComPtr<IMMDeviceCollection> collection;
  enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
  if (!collection.Get())
    return std::string();

  UINT count = 0;
  collection->GetCount(&count);
  ComPtr<IMMDevice> output_device;
  for (UINT i = 0; i < count; ++i) {
    collection->Item(i, &output_device);
    std::string output_controller_id(
        GetAudioControllerID(output_device.Get(), enumerator.Get()));
    if (output_controller_id == controller_id)
      break;
    output_device = nullptr;
  }

  return output_device.Get() ? GetDeviceID(output_device.Get()) : std::string();
}

std::string CoreAudioUtil::GetFriendlyName(const std::string& device_id,
                                           EDataFlow data_flow,
                                           ERole role) {
  ComPtr<IMMDevice> audio_device = CreateDevice(device_id, data_flow, role);
  if (!audio_device.Get())
    return std::string();

  AudioDeviceName device_name;
  HRESULT hr = GetDeviceName(audio_device.Get(), &device_name);
  if (FAILED(hr))
    return std::string();

  return device_name.device_name;
}

EDataFlow CoreAudioUtil::GetDataFlow(IMMDevice* device) {
  ComPtr<IMMEndpoint> endpoint;
  HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&endpoint));
  if (FAILED(hr)) {
    DVLOG(1) << "IMMDevice::QueryInterface: " << std::hex << hr;
    return eAll;
  }

  EDataFlow data_flow;
  hr = endpoint->GetDataFlow(&data_flow);
  if (FAILED(hr)) {
    DVLOG(1) << "IMMEndpoint::GetDataFlow: " << std::hex << hr;
    return eAll;
  }
  return data_flow;
}

ComPtr<IMMDevice> CoreAudioUtil::CreateDevice(const std::string& device_id,
                                              EDataFlow data_flow,
                                              ERole role) {
  return CreateDeviceInternal(device_id, data_flow, role);
}

ComPtr<IAudioClient> CoreAudioUtil::CreateClient(const std::string& device_id,
                                                 EDataFlow data_flow,
                                                 ERole role) {
  ComPtr<IMMDevice> device(CreateDevice(device_id, data_flow, role));
  return CreateClientInternal(device.Get());
}

ComPtr<IAudioClient3> CoreAudioUtil::CreateClient3(const std::string& device_id,
                                                   EDataFlow data_flow,
                                                   ERole role) {
  ComPtr<IMMDevice> device(CreateDevice(device_id, data_flow, role));
  return CreateClientInternal3(device.Get());
}

HRESULT CoreAudioUtil::GetSharedModeMixFormat(IAudioClient* client,
                                              WAVEFORMATEXTENSIBLE* format) {
  // The GetMixFormat method retrieves the stream format that the audio engine
  // uses for its internal processing of shared-mode streams. The method
  // allocates the storage for the structure and this memory will be released
  // when |mix_format| goes out of scope. The GetMixFormat method retrieves a
  // format descriptor that is in the form of a WAVEFORMATEXTENSIBLE structure
  // instead of a standalone WAVEFORMATEX structure. The method outputs a
  // pointer to the WAVEFORMATEX structure that is embedded at the start of
  // this WAVEFORMATEXTENSIBLE structure.
  // Note that, crbug/803056 indicates that some devices can return a format
  // where only the WAVEFORMATEX parts is initialized and we must be able to
  // account for that.
  ScopedCoMem<WAVEFORMATEXTENSIBLE> mix_format;
  HRESULT hr =
      client->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&mix_format));
  if (FAILED(hr))
    return hr;

  // Use a wave format wrapper to make things simpler.
  WaveFormatWrapper wrapped_format(mix_format.get());

  // Verify that the reported format can be mixed by the audio engine in
  // shared mode.
  if (!wrapped_format.IsPcm() && !wrapped_format.IsFloat()) {
    DLOG(ERROR)
        << "Only pure PCM or float audio streams can be mixed in shared mode";
    return AUDCLNT_E_UNSUPPORTED_FORMAT;
  }
  // Log a warning for the rare case where |mix_format| only contains a
  // stand-alone WAVEFORMATEX structure but don't return.
  if (!wrapped_format.IsExtensible()) {
    DLOG(WARNING) << "The returned format contains no extended information. "
                     "The size is "
                  << wrapped_format.size() << " bytes.";
  }

  // Copy the correct number of bytes into |*format| taking into account if
  // the returned structure is correctly extended or not.
  CHECK_LE(wrapped_format.size(), sizeof(WAVEFORMATEXTENSIBLE))
      << "Format tag: 0x" << std::hex << wrapped_format->wFormatTag;
  memcpy(format, wrapped_format.get(), wrapped_format.size());
  DVLOG(2) << CoreAudioUtil::WaveFormatToString(format);

  return hr;
}

bool CoreAudioUtil::IsFormatSupported(IAudioClient* client,
                                      AUDCLNT_SHAREMODE share_mode,
                                      const WaveFormatWrapper format) {
  ScopedCoMem<WAVEFORMATEX> closest_match;
  HRESULT hr = client->IsFormatSupported(share_mode, format, &closest_match);

  // This log can only be triggered for shared mode.
  DLOG_IF(ERROR, hr == S_FALSE) << "Format is not supported "
                                << "but a closest match exists.";
  // This log can be triggered both for shared and exclusive modes.
  DLOG_IF(ERROR, hr == AUDCLNT_E_UNSUPPORTED_FORMAT) << "Unsupported format.";
  DVLOG(2) << CoreAudioUtil::WaveFormatToString(format);
  if (hr == S_FALSE) {
    DVLOG(2) << CoreAudioUtil::WaveFormatToString(closest_match.get());
  }

  return (hr == S_OK);
}

bool CoreAudioUtil::IsChannelLayoutSupported(const std::string& device_id,
                                             EDataFlow data_flow,
                                             ERole role,
                                             ChannelLayout channel_layout) {
  // First, get the preferred mixing format for shared mode streams.
  ComPtr<IAudioClient> client(CreateClient(device_id, data_flow, role));
  if (!client.Get())
    return false;

  WAVEFORMATEXTENSIBLE mix_format;
  HRESULT hr = GetSharedModeMixFormat(client.Get(), &mix_format);
  if (FAILED(hr))
    return false;

  // Next, check if it is possible to use an alternative format where the
  // channel layout (and possibly number of channels) is modified.

  // Convert generic channel layout into Windows-specific channel configuration
  // but only if the wave format is extended (can contain a channel mask).
  WaveFormatWrapper format(&mix_format);
  if (format.IsExtensible()) {
    ChannelConfig new_config = ChannelLayoutToChannelConfig(channel_layout);
    if (new_config == KSAUDIO_SPEAKER_UNSUPPORTED) {
      return false;
    }
    format.GetExtensible()->dwChannelMask = new_config;
  }

  // Modify the format if the new channel layout has changed the number of
  // utilized channels.
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  if (channels != format->nChannels) {
    format->nChannels = channels;
    format->nBlockAlign = (format->wBitsPerSample / 8) * channels;
    format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  }
  DVLOG(2) << CoreAudioUtil::WaveFormatToString(format);

  // Some devices can initialize a shared-mode stream with a format that is
  // not identical to the mix format obtained from the GetMixFormat() method.
  // However, chances of succeeding increases if we use the same number of
  // channels and the same sample rate as the mix format. I.e, this call will
  // return true only in those cases where the audio engine is able to support
  // an even wider range of shared-mode formats where the installation package
  // for the audio device includes a local effects (LFX) audio processing
  // object (APO) that can handle format conversions.
  return CoreAudioUtil::IsFormatSupported(client.Get(),
                                          AUDCLNT_SHAREMODE_SHARED, format);
}

HRESULT CoreAudioUtil::GetDevicePeriod(IAudioClient* client,
                                       AUDCLNT_SHAREMODE share_mode,
                                       REFERENCE_TIME* device_period) {
  // Get the period of the engine thread.
  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME minimum_period = 0;
  HRESULT hr = client->GetDevicePeriod(&default_period, &minimum_period);
  if (FAILED(hr))
    return hr;

  *device_period = (share_mode == AUDCLNT_SHAREMODE_SHARED) ? default_period
                                                            : minimum_period;
  DVLOG(2) << "device_period: "
           << ReferenceTimeToTimeDelta(*device_period).InMillisecondsF()
           << " [ms]";
  return hr;
}

HRESULT CoreAudioUtil::GetPreferredAudioParameters(const std::string& device_id,
                                                   bool is_output_device,
                                                   AudioParameters* params,
                                                   bool is_offload_stream) {
  // Loopback audio streams must be input streams.
  DCHECK(!(AudioDeviceDescription::IsLoopbackDevice(device_id) &&
           is_output_device));
  if (AudioDeviceDescription::IsLoopbackDevice(device_id) && is_output_device) {
    LOG(WARNING) << "Loopback device must be an input device";
    return E_FAIL;
  }

  ComPtr<IMMDevice> device(CreateDeviceByID(device_id, is_output_device));
  if (!device.Get())
    return E_FAIL;

  ComPtr<IAudioClient> client(CreateClientInternal(device.Get()));
  if (!client.Get())
    return E_FAIL;

  bool attempt_audio_offload =
      is_offload_stream && EnableOffloadForClient(client.Get());

  HRESULT hr = GetPreferredAudioParametersInternal(
      client.Get(), is_output_device, params, attempt_audio_offload);
  if (FAILED(hr) || is_output_device || !params->IsValid()) {
    return hr;
  }

  // The following functionality is only for input devices.
  DCHECK(!is_output_device);

  // TODO(dalecurtis): Old code rewrote != 1 channels to stereo, do we still
  // need to do the same thing?
  if (params->channels() != 1 &&
      params->channel_layout() != CHANNEL_LAYOUT_DISCRETE) {
    DLOG(WARNING)
        << "Replacing existing audio parameter with predefined version";
    params->Reset(params->format(), media::ChannelLayoutConfig::Stereo(),
                  params->sample_rate(), params->frames_per_buffer());
  }

  return hr;
}

ChannelConfig CoreAudioUtil::GetChannelConfig(const std::string& device_id,
                                              EDataFlow data_flow) {
  const ERole role = AudioDeviceDescription::IsCommunicationsDevice(device_id)
                         ? eCommunications
                         : eConsole;
  ComPtr<IAudioClient> client(CreateClient(device_id, data_flow, role));

  WAVEFORMATEXTENSIBLE mix_format;
  if (!client.Get() ||
      FAILED(GetSharedModeMixFormat(client.Get(), &mix_format)))
    return 0;
  WaveFormatWrapper format(&mix_format);
  if (!format.IsExtensible()) {
    // A format descriptor from WAVEFORMATEX only supports mono and stereo.
    DCHECK_LE(format->nChannels, 2);
    DVLOG(1) << "Format does not contain any channel mask."
             << " Guessing layout by channel count: " << std::dec
             << format->nChannels;
    return GuessChannelConfig(format->nChannels);
  }

  return static_cast<ChannelConfig>(format.GetExtensible()->dwChannelMask);
}

HRESULT CoreAudioUtil::SharedModeInitialize(IAudioClient* client,
                                            const WaveFormatWrapper format,
                                            HANDLE event_handle,
                                            uint32_t requested_buffer_size,
                                            uint32_t* endpoint_buffer_size,
                                            const GUID* session_guid,
                                            bool is_offload_stream) {
  // Use default flags (i.e, dont set AUDCLNT_STREAMFLAGS_NOPERSIST) to
  // ensure that the volume level and muting state for a rendering session
  // are persistent across system restarts. The volume level and muting
  // state for a capture session are never persistent.
  DWORD stream_flags = 0;

  // Enable event-driven streaming if a valid event handle is provided.
  // After the stream starts, the audio engine will signal the event handle
  // to notify the client each time a buffer becomes ready to process.
  // Event-driven buffering is supported for both rendering and capturing.
  // Both shared-mode and exclusive-mode streams can use event-driven
  // buffering.
  bool use_event =
      (event_handle != NULL && event_handle != INVALID_HANDLE_VALUE);
  if (use_event)
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  DVLOG(2) << "stream_flags: 0x" << std::hex << stream_flags;

  const bool supports_iac3 = IAudioClient3IsSupported();

  HRESULT hr;

  if (is_offload_stream) {
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags,
                            GetOffloadBufferTimeIn100Ns(), 0, format,
                            session_guid);
    // Typically GetBufferSize() must be called after successfully
    // initialization. AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED is the only case we
    // allow with an initialization failure.
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
      uint32_t buffer_size_in_frames = 0;
      hr = client->GetBufferSize(&buffer_size_in_frames);
      if (SUCCEEDED(hr)) {
        REFERENCE_TIME buffer_duration_in_ns = BufferSizeInFramesToTimeDelta(
            buffer_size_in_frames, format->nAvgBytesPerSec,
            format->nBlockAlign);
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags,
                                buffer_duration_in_ns, 0, format, session_guid);
      }
    }
  } else if (supports_iac3 && requested_buffer_size > 0) {
    // Try to obtain an IAudioClient3 interface from the IAudioClient object.
    // Use ComPtr::As for doing QueryInterface calls on COM objects.
    ComPtr<IAudioClient> audio_client(client);
    ComPtr<IAudioClient3> audio_client_3;
    hr = audio_client.As(&audio_client_3);
    if (FAILED(hr)) {
      DVLOG(1) << "Failed to obtain IAudioClient3 interface: " << std::hex
               << hr;
      return hr;
    }
    // Initialize a low-latency client using IAudioClient3.
    hr = audio_client_3->InitializeSharedAudioStream(
        stream_flags, requested_buffer_size, format, session_guid);
    if (FAILED(hr)) {
      DVLOG(1) << "IAudioClient3::InitializeSharedAudioStream: " << std::hex
               << hr;
      return hr;
    }
  } else {
    // Initialize the shared mode client for minimal delay.
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags, 0, 0,
                            format, session_guid);
    if (FAILED(hr)) {
      DVLOG(1) << "IAudioClient::Initialize: " << std::hex << hr;
      return hr;
    }
  }

  if (use_event) {
    hr = client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
      DVLOG(1) << "IAudioClient::SetEventHandle: " << std::hex << hr;
      return hr;
    }
  }

  UINT32 buffer_size_in_frames = 0;
  hr = client->GetBufferSize(&buffer_size_in_frames);
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetBufferSize: " << std::hex << hr;
    return hr;
  }

  *endpoint_buffer_size = buffer_size_in_frames;
  DVLOG(2) << "endpoint buffer size: " << buffer_size_in_frames;

  // TODO(henrika): utilize when delay measurements are added.
  REFERENCE_TIME latency = 0;
  hr = client->GetStreamLatency(&latency);
  DVLOG(2) << "stream latency: "
           << ReferenceTimeToTimeDelta(latency).InMillisecondsF() << " [ms]";
  return hr;
}

ComPtr<IAudioRenderClient> CoreAudioUtil::CreateRenderClient(
    IAudioClient* client) {
  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  ComPtr<IAudioRenderClient> audio_render_client;
  HRESULT hr = client->GetService(IID_PPV_ARGS(&audio_render_client));
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetService: " << std::hex << hr;
    return ComPtr<IAudioRenderClient>();
  }
  return audio_render_client;
}

ComPtr<IAudioCaptureClient> CoreAudioUtil::CreateCaptureClient(
    IAudioClient* client) {
  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from a capturing endpoint buffer.
  ComPtr<IAudioCaptureClient> audio_capture_client;
  HRESULT hr = client->GetService(IID_PPV_ARGS(&audio_capture_client));
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetService: " << std::hex << hr;
    return ComPtr<IAudioCaptureClient>();
  }
  return audio_capture_client;
}

bool CoreAudioUtil::FillRenderEndpointBufferWithSilence(
    IAudioClient* client,
    IAudioRenderClient* render_client) {
  UINT32 endpoint_buffer_size = 0;
  if (FAILED(client->GetBufferSize(&endpoint_buffer_size))) {
    PLOG(ERROR) << "Failed IAudioClient::GetBufferSize()";
    return false;
  }

  UINT32 num_queued_frames = 0;
  if (FAILED(client->GetCurrentPadding(&num_queued_frames))) {
    PLOG(ERROR) << "Failed IAudioClient::GetCurrentPadding()";
    return false;
  }

  BYTE* data = NULL;
  int num_frames_to_fill = endpoint_buffer_size - num_queued_frames;
  if (FAILED(render_client->GetBuffer(num_frames_to_fill, &data))) {
    PLOG(ERROR) << "Failed IAudioRenderClient::GetBuffer()";
    return false;
  }

  // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
  // explicitly write silence data to the rendering buffer.
  if (FAILED(render_client->ReleaseBuffer(num_frames_to_fill,
                                          AUDCLNT_BUFFERFLAGS_SILENT))) {
    PLOG(ERROR) << "Failed IAudioRenderClient::ReleaseBuffer()";
    return false;
  }

  return true;
}

// static
bool CoreAudioUtil::EnableOffloadForClient(IAudioClient* client) {
  ComPtr<IAudioClient> audio_client(client);
  ComPtr<IAudioClient2> audio_client2;

  if (!CoreAudioUtil::IsAudioOffloadSupported(audio_client.Get())) {
    return false;
  }
  HRESULT hr = audio_client.As(&audio_client2);
  if (SUCCEEDED(hr)) {
    AudioClientProperties client_properties = {0};
    client_properties.cbSize = sizeof(AudioClientProperties);
    client_properties.bIsOffload = true;
    client_properties.eCategory = AudioCategory_Media;

    hr = audio_client2->SetClientProperties(&client_properties);
    if (SUCCEEDED(hr)) {
      DVLOG(1) << "Enabled audio offload on the client.";
      return true;
    }
  }

  return false;
}

// static
bool CoreAudioUtil::IsAudioOffloadSupported(IAudioClient* client) {
  if (!base::FeatureList::IsEnabled(kAudioOffload)) {
    return false;
  } else if (!client) {
    // If no client is specified, we can't determine if offload is supported,
    // thus allow audio offload to be attempted, the real capability will be
    // checked when the audio client is created.
    return true;
  }

  ComPtr<IAudioClient> audio_client(client);
  ComPtr<IAudioClient2> audio_client2;
  BOOL is_offloadable = FALSE;

  HRESULT hr = audio_client.As(&audio_client2);
  if (SUCCEEDED(hr)) {
    hr = audio_client2->IsOffloadCapable(AudioCategory_Media, &is_offloadable);
    return (hr == S_OK && (is_offloadable == TRUE));
  }

  return false;
}

}  // namespace media
