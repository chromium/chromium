// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/core_audio_util_win.h"

#include <devicetopology.h>
#include <functiondiscoverykeys_devpkey.h>
#include <objbase.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/scoped_variant.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"

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

enum { KSAUDIO_SPEAKER_UNSUPPORTED = 0 };

// Used for mapping UMA histograms with corresponding source of logging.
enum class UmaLogStep {
  CREATE_DEVICE_ENUMERATOR,
  CREATE_DEVICE,
  CREATE_CLIENT,
  GET_MIX_FORMAT,
  GET_DEVICE_PERIOD,
  GET_SHARED_MODE_ENGINE_PERIOD,
};

using UMALogCallback = base::RepeatingCallback<void(UmaLogStep, HRESULT)>;

// Empty UMA logging callback to be passed to functions that don't need to log
// any UMA stats
void LogUMAEmptyCb(UmaLogStep step, HRESULT hr) {}

// UMA logging callback used for tracking return values of
// GetPreferredAudioParameters for output stream proxy parameter creation, in
// order to get a clearer picture of the different failure reasons and their
// distribution. https://crbug.com/774998
void LogUMAPreferredOutputParams(UmaLogStep step, HRESULT hr) {
  switch (step) {
    case UmaLogStep::CREATE_DEVICE_ENUMERATOR:
      base::UmaHistogramSparse(
          "Media.AudioOutputStreamProxy."
          "GetPreferredOutputStreamParametersWin.CreateDeviceEnumeratorResult",
          hr);
      break;
    case UmaLogStep::CREATE_DEVICE:
      base::UmaHistogramSparse(
          "Media.AudioOutputStreamProxy."
          "GetPreferredOutputStreamParametersWin.CreateDeviceResult",
          hr);
      break;
    case UmaLogStep::CREATE_CLIENT:
      base::UmaHistogramSparse(
          "Media.AudioOutputStreamProxy."
          "GetPreferredOutputStreamParametersWin.CreateClientResult",
          hr);
      break;
    case UmaLogStep::GET_MIX_FORMAT:
      base::UmaHistogramSparse(
          "Media.AudioOutputStreamProxy."
          "GetPreferredOutputStreamParametersWin.GetMixFormatResult",
          hr);
      break;
    case UmaLogStep::GET_DEVICE_PERIOD:
      base::UmaHistogramSparse(
          "Media.AudioOutputStreamProxy."
          "GetPreferredOutputStreamParametersWin.GetDevicePeriodResult",
          hr);
      break;
    case UmaLogStep::GET_SHARED_MODE_ENGINE_PERIOD:
      // TODO(crbug.com/892044): add histogram logging.
      break;
  }
}

// Converts Microsoft's channel configuration to ChannelLayout.
// This mapping is not perfect but the best we can do given the current
// ChannelLayout enumerator and the Windows-specific speaker configurations
// defined in ksmedia.h. Don't assume that the channel ordering in
// ChannelLayout is exactly the same as the Windows specific configuration.
// As an example: KSAUDIO_SPEAKER_7POINT1_SURROUND is mapped to
// CHANNEL_LAYOUT_7_1 but the positions of Back L, Back R and Side L, Side R
// speakers are different in these two definitions.
ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config) {
  switch (config) {
    case KSAUDIO_SPEAKER_MONO:
      DVLOG(2) << "KSAUDIO_SPEAKER_MONO=>CHANNEL_LAYOUT_MONO";
      return CHANNEL_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:
      DVLOG(2) << "KSAUDIO_SPEAKER_STEREO=>CHANNEL_LAYOUT_STEREO";
      return CHANNEL_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD:
      DVLOG(2) << "KSAUDIO_SPEAKER_QUAD=>CHANNEL_LAYOUT_QUAD";
      return CHANNEL_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_SURROUND=>CHANNEL_LAYOUT_4_0";
      return CHANNEL_LAYOUT_4_0;
    case KSAUDIO_SPEAKER_5POINT1:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1=>CHANNEL_LAYOUT_5_1_BACK";
      return CHANNEL_LAYOUT_5_1_BACK;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1_SURROUND=>CHANNEL_LAYOUT_5_1";
      return CHANNEL_LAYOUT_5_1;
    case KSAUDIO_SPEAKER_7POINT1:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1=>CHANNEL_LAYOUT_7_1_WIDE";
      return CHANNEL_LAYOUT_7_1_WIDE;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1_SURROUND=>CHANNEL_LAYOUT_7_1";
      return CHANNEL_LAYOUT_7_1;
    case KSAUDIO_SPEAKER_DIRECTOUT:
      // When specifying the wave format for a direct-out stream, an application
      // should set the dwChannelMask member of the WAVEFORMATEXTENSIBLE
      // structure to the value KSAUDIO_SPEAKER_DIRECTOUT, which is zero.
      // A channel mask of zero indicates that no speaker positions are defined.
      // As always, the number of channels in the stream is specified in the
      // Format.nChannels member.
      DVLOG(2) << "KSAUDIO_SPEAKER_DIRECTOUT=>CHANNEL_LAYOUT_DISCRETE";
      return CHANNEL_LAYOUT_DISCRETE;
    default:
      DVLOG(2) << "Unsupported channel configuration: " << config;
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
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
    case CHANNEL_LAYOUT_QUAD:
      DVLOG(2) << "CHANNEL_LAYOUT_QUAD=>KSAUDIO_SPEAKER_QUAD";
      return KSAUDIO_SPEAKER_QUAD;
    case CHANNEL_LAYOUT_4_0:
      DVLOG(2) << "CHANNEL_LAYOUT_4_0=>KSAUDIO_SPEAKER_SURROUND";
      return KSAUDIO_SPEAKER_SURROUND;
    case CHANNEL_LAYOUT_5_1_BACK:
      DVLOG(2) << "CHANNEL_LAYOUT_5_1_BACK=>KSAUDIO_SPEAKER_5POINT1";
      return KSAUDIO_SPEAKER_5POINT1;
    case CHANNEL_LAYOUT_5_1:
      DVLOG(2) << "CHANNEL_LAYOUT_5_1=>KSAUDIO_SPEAKER_5POINT1_SURROUND";
      return KSAUDIO_SPEAKER_5POINT1_SURROUND;
    case CHANNEL_LAYOUT_7_1_WIDE:
      DVLOG(2) << "CHANNEL_LAYOUT_7_1_WIDE=>KSAUDIO_SPEAKER_7POINT1";
      return KSAUDIO_SPEAKER_7POINT1;
    case CHANNEL_LAYOUT_7_1:
      DVLOG(2) << "CHANNEL_LAYOUT_7_1=>KSAUDIO_SPEAKER_7POINT1_SURROUND";
      return KSAUDIO_SPEAKER_7POINT1_SURROUND;
    default:
      DVLOG(2) << "Unsupported channel layout: " << layout;
      return KSAUDIO_SPEAKER_UNSUPPORTED;
  }
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
  HRESULT hr = device->OpenPropertyStore(STGM_READ, properties.GetAddressOf());
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
    bool allow_reinitialize,
    const UMALogCallback& uma_log_cb) {
  ComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&device_enumerator));
  if (hr == CO_E_NOTINITIALIZED && allow_reinitialize) {
    LOG(ERROR) << "CoCreateInstance fails with CO_E_NOTINITIALIZED";
    // We have seen crashes which indicates that this method can in fact
    // fail with CO_E_NOTINITIALIZED in combination with certain 3rd party
    // modules. Calling CoInitializeEx is an attempt to resolve the reported
    // issues. See http://crbug.com/378465 for details.
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
      hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&device_enumerator));
    }
  }
  uma_log_cb.Run(UmaLogStep::CREATE_DEVICE_ENUMERATOR, hr);
  return device_enumerator;
}

ChannelLayout GetChannelLayout(const WAVEFORMATPCMEX& mix_format) {
  // Get the integer mask which corresponds to the channel layout the
  // audio engine uses for its internal processing/mixing of shared-mode
  // streams. This mask indicates which channels are present in the multi-
  // channel stream. The least significant bit corresponds with the Front
  // Left speaker, the next least significant bit corresponds to the Front
  // Right speaker, and so on, continuing in the order defined in KsMedia.h.
  // See
  // http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083.aspx
  // for more details.
  ChannelConfig channel_config = mix_format.dwChannelMask;

  // Convert Microsoft's channel configuration to generic ChannelLayout.
  ChannelLayout channel_layout = ChannelConfigToChannelLayout(channel_config);

  // Some devices don't appear to set a valid channel layout, so guess based
  // on the number of channels.  See http://crbug.com/311906.
  if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
    DVLOG(1) << "Unsupported channel config: " << std::hex << channel_config
             << ".  Guessing layout by channel count: " << std::dec
             << mix_format.Format.nChannels;
    channel_layout = GuessChannelLayout(mix_format.Format.nChannels);
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
      CreateDeviceEnumeratorInternal(false,
                                     base::BindRepeating(&LogUMAEmptyCb));
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
                                       ERole role,
                                       const UMALogCallback& uma_log_cb) {
  ComPtr<IMMDevice> endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ComPtr<IMMDeviceEnumerator> device_enum(
      CreateDeviceEnumeratorInternal(true, uma_log_cb));
  if (!device_enum.Get())
    return endpoint_device;

  HRESULT hr;
  if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
    hr = device_enum->GetDefaultAudioEndpoint(data_flow, role,
                                              endpoint_device.GetAddressOf());
  } else {
    hr = device_enum->GetDevice(base::UTF8ToUTF16(device_id).c_str(),
                                endpoint_device.GetAddressOf());
  }
  DVLOG_IF(1, FAILED(hr)) << "Create Device failed: " << std::hex << hr;

  // Verify that the audio endpoint device is active, i.e., that the audio
  // adapter that connects to the endpoint device is present and enabled.
  if (SUCCEEDED(hr) && !IsDeviceActive(endpoint_device.Get())) {
    DVLOG(1) << "Selected endpoint device is not active";
    endpoint_device.Reset();
    hr = E_FAIL;
  }

  uma_log_cb.Run(UmaLogStep::CREATE_DEVICE, hr);
  return endpoint_device;
}

// Decide on data_flow and role based on |device_id|, and return the
// corresponding audio device.
ComPtr<IMMDevice> CreateDeviceByID(const std::string& device_id,
                                   bool is_output_device,
                                   const UMALogCallback& uma_log_cb) {
  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    DCHECK(!is_output_device);
    return CreateDeviceInternal(AudioDeviceDescription::kDefaultDeviceId,
                                eRender, eConsole, uma_log_cb);
  }

  EDataFlow data_flow = is_output_device ? eRender : eCapture;
  if (device_id == AudioDeviceDescription::kCommunicationsDeviceId)
    return CreateDeviceInternal(AudioDeviceDescription::kDefaultDeviceId,
                                data_flow, eCommunications, uma_log_cb);

  // If AudioDeviceDescription::IsDefaultDevice(device_id), a default device
  // will be created
  return CreateDeviceInternal(device_id, data_flow, eConsole, uma_log_cb);
}

// Creates and activates an IAudioClient COM object given the selected
// endpoint device.
ComPtr<IAudioClient> CreateClientInternal(IMMDevice* audio_device,
                                          const UMALogCallback& uma_log_cb) {
  if (!audio_device)
    return ComPtr<IAudioClient>();

  ComPtr<IAudioClient> audio_client;
  HRESULT hr = audio_device->Activate(
      __uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, &audio_client);
  DVLOG_IF(1, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  uma_log_cb.Run(UmaLogStep::CREATE_CLIENT, hr);
  return audio_client;
}

HRESULT GetPreferredAudioParametersInternal(IAudioClient* client,
                                            bool is_output_device,
                                            AudioParameters* params,
                                            const UMALogCallback& uma_log_cb) {
  WAVEFORMATPCMEX mix_format;
  HRESULT hr = CoreAudioUtil::GetSharedModeMixFormat(client, &mix_format);
  uma_log_cb.Run(UmaLogStep::GET_MIX_FORMAT, hr);
  if (FAILED(hr))
    return hr;

  // Preferred sample rate.
  int sample_rate = mix_format.Format.nSamplesPerSec;

  int min_frames_per_buffer = 0;
  int max_frames_per_buffer = 0;
  int frames_per_buffer;

  ComPtr<IAudioClient3> audio_client_3;
  hr = client->QueryInterface(audio_client_3.GetAddressOf());
  if (SUCCEEDED(hr)) {
    UINT32 default_period_frames;
    UINT32 fundamental_period_frames;
    UINT32 min_period_frames;
    UINT32 max_period_frames;
    hr = audio_client_3->GetSharedModeEnginePeriod(
        &(mix_format.Format), &default_period_frames,
        &fundamental_period_frames, &min_period_frames, &max_period_frames);

    uma_log_cb.Run(UmaLogStep::GET_SHARED_MODE_ENGINE_PERIOD, hr);
    if (SUCCEEDED(hr)) {
      min_frames_per_buffer = min_period_frames;
      max_frames_per_buffer = max_period_frames;
      frames_per_buffer = default_period_frames;
    }
  }

  // If we don't have access to IAudioClient3 or if the call to
  // GetSharedModeEnginePeriod() fails we fall back to GetDevicePeriod().
  if (FAILED(hr)) {
    REFERENCE_TIME default_period = 0;
    hr = CoreAudioUtil::GetDevicePeriod(client, AUDCLNT_SHAREMODE_SHARED,
                                        &default_period);
    uma_log_cb.Run(UmaLogStep::GET_DEVICE_PERIOD, hr);
    if (FAILED(hr))
      return hr;

    // We are using the native device period to derive the smallest possible
    // buffer size in shared mode. Note that the actual endpoint buffer will be
    // larger than this size but it will be possible to fill it up in two calls.
    // TODO(henrika): ensure that this scheme works for capturing as well.
    frames_per_buffer = static_cast<int>(
        sample_rate * CoreAudioUtil::ReferenceTimeToTimeDelta(default_period)
                          .InSecondsF() +
        0.5);
  }

  ChannelLayout channel_layout = GetChannelLayout(mix_format);
  AudioParameters audio_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, sample_rate,
      frames_per_buffer,
      AudioParameters::HardwareCapabilities(min_frames_per_buffer,
                                            max_frames_per_buffer));
  // Set the number of channels explicitly to two for input devices if
  // the channel layout is discrete to ensure that the parameters are valid
  // and that clients does not have to support multi-channel input cases.
  // Any required down-mixing from N (N > 2) to 2 must be performed by the
  // input stream implementation instead.
  // See https://crbug/868026 for examples where this approach is needed.
  if (!is_output_device &&
      audio_params.channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
    DLOG(WARNING)
        << "Forcing number of channels to 2 for CHANNEL_LAYOUT_DISCRETE";
    audio_params.set_channels_for_discrete(2);
  }
  DCHECK(audio_params.IsValid());
  *params = audio_params;
  DVLOG(1) << params->AsHumanReadableString();

  return hr;
}

}  // namespace

bool CoreAudioUtil::IsSupported() {
  static bool g_is_supported = IsSupportedInternal();
  return g_is_supported;
}

std::string CoreAudioUtil::WaveFormatExToString(
    const WAVEFORMATEXTENSIBLE* format) {
  DCHECK_EQ(format->Format.wFormatTag, WAVE_FORMAT_EXTENSIBLE);
  DCHECK_GE(format->Format.cbSize, 22);
  std::string wave_format = base::StringPrintf(
      "wFormatTag: WAVE_FORMAT_EXTENSIBLE, nChannels: %d, nSamplesPerSec: %lu"
      ", nAvgBytesPerSec: %lu, nBlockAlign: %d, wBitsPerSample: %d, cbSize: %d"
      ", wValidBitsPerSample: %d, dwChannelMask: 0x%lX",
      format->Format.nChannels, format->Format.nSamplesPerSec,
      format->Format.nAvgBytesPerSec, format->Format.nBlockAlign,
      format->Format.wBitsPerSample, format->Format.cbSize,
      format->Samples.wValidBitsPerSample, format->dwChannelMask);
  if (format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
    base::StringAppendF(&wave_format, "%s",
                        ", SubFormat: KSDATAFORMAT_SUBTYPE_PCM");
  } else if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
    base::StringAppendF(&wave_format, "%s",
                        ", SubFormat: KSDATAFORMAT_SUBTYPE_IEEE_FLOAT");
  } else {
    base::StringAppendF(&wave_format, "%s", ", SubFormat: NOT_SUPPORTED");
  }
  return wave_format;
}

base::TimeDelta CoreAudioUtil::ReferenceTimeToTimeDelta(REFERENCE_TIME time) {
  // Each unit of reference time is 100 nanoseconds <=> 0.1 microsecond.
  return base::TimeDelta::FromMicroseconds(0.1 * time + 0.5);
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
      data_flow, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
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
  return CreateDeviceEnumeratorInternal(true,
                                        base::BindRepeating(&LogUMAEmptyCb));
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
      FAILED(topology->GetConnector(0, connector.GetAddressOf())) ||
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
  if (FAILED(enumerator->GetDevice(filter_id, device_node.GetAddressOf())) ||
      FAILED(device_node->OpenPropertyStore(STGM_READ,
                                            properties.GetAddressOf())) ||
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
  enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                 collection.GetAddressOf());
  if (!collection.Get())
    return std::string();

  UINT count = 0;
  collection->GetCount(&count);
  ComPtr<IMMDevice> output_device;
  for (UINT i = 0; i < count; ++i) {
    collection->Item(i, output_device.GetAddressOf());
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
  HRESULT hr = device->QueryInterface(endpoint.GetAddressOf());
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
  return CreateDeviceInternal(device_id, data_flow, role,
                              base::BindRepeating(&LogUMAEmptyCb));
}

ComPtr<IAudioClient> CoreAudioUtil::CreateClient(const std::string& device_id,
                                                 EDataFlow data_flow,
                                                 ERole role) {
  ComPtr<IMMDevice> device(CreateDevice(device_id, data_flow, role));

  return CreateClientInternal(device.Get(),
                              base::BindRepeating(&LogUMAEmptyCb));
}

HRESULT CoreAudioUtil::GetSharedModeMixFormat(
    IAudioClient* client, WAVEFORMATPCMEX* format) {
  VLOG(1) << __FUNCTION__;
  ScopedCoMem<WAVEFORMATPCMEX> format_pcmex;
  HRESULT hr = client->GetMixFormat(
      reinterpret_cast<WAVEFORMATEX**>(&format_pcmex));
  if (FAILED(hr))
    return hr;

  size_t bytes = sizeof(WAVEFORMATEX) + format_pcmex->Format.cbSize;
  DCHECK_EQ(bytes, sizeof(WAVEFORMATPCMEX))
      << "Format tag: 0x" << std::hex << format_pcmex->Format.wFormatTag;

  memcpy(format, format_pcmex, bytes);
  DVLOG(2) << CoreAudioUtil::WaveFormatExToString(format);

  return hr;
}

bool CoreAudioUtil::IsFormatSupported(IAudioClient* client,
                                      AUDCLNT_SHAREMODE share_mode,
                                      const WAVEFORMATPCMEX* format) {
  ScopedCoMem<WAVEFORMATEXTENSIBLE> closest_match;
  HRESULT hr = client->IsFormatSupported(
      share_mode, reinterpret_cast<const WAVEFORMATEX*>(format),
      reinterpret_cast<WAVEFORMATEX**>(&closest_match));

  // This log can only be triggered for shared mode.
  DLOG_IF(ERROR, hr == S_FALSE) << "Format is not supported "
                                << "but a closest match exists.";
  // This log can be triggered both for shared and exclusive modes.
  DLOG_IF(ERROR, hr == AUDCLNT_E_UNSUPPORTED_FORMAT) << "Unsupported format.";
  if (hr == S_FALSE) {
    DVLOG(2) << CoreAudioUtil::WaveFormatExToString(closest_match);
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

  WAVEFORMATPCMEX format;
  HRESULT hr = GetSharedModeMixFormat(client.Get(), &format);
  if (FAILED(hr))
    return false;

  // Next, check if it is possible to use an alternative format where the
  // channel layout (and possibly number of channels) is modified.

  // Convert generic channel layout into Windows-specific channel configuration.
  ChannelConfig new_config = ChannelLayoutToChannelConfig(channel_layout);
  if (new_config == KSAUDIO_SPEAKER_UNSUPPORTED) {
    return false;
  }
  format.dwChannelMask = new_config;

  // Modify the format if the new channel layout has changed the number of
  // utilized channels.
  const int channels = ChannelLayoutToChannelCount(channel_layout);
  if (channels != format.Format.nChannels) {
    format.Format.nChannels = channels;
    format.Format.nBlockAlign = (format.Format.wBitsPerSample / 8) * channels;
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec *
                                    format.Format.nBlockAlign;
  }
  DVLOG(2) << CoreAudioUtil::WaveFormatExToString(&format);

  // Some devices can initialize a shared-mode stream with a format that is
  // not identical to the mix format obtained from the GetMixFormat() method.
  // However, chances of succeeding increases if we use the same number of
  // channels and the same sample rate as the mix format. I.e, this call will
  // return true only in those cases where the audio engine is able to support
  // an even wider range of shared-mode formats where the installation package
  // for the audio device includes a local effects (LFX) audio processing
  // object (APO) that can handle format conversions.
  return CoreAudioUtil::IsFormatSupported(client.Get(),
                                          AUDCLNT_SHAREMODE_SHARED, &format);
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

  *device_period = (share_mode == AUDCLNT_SHAREMODE_SHARED) ? default_period :
      minimum_period;
  DVLOG(2) << "device_period: "
           << ReferenceTimeToTimeDelta(*device_period).InMillisecondsF()
           << " [ms]";
  return hr;
}

HRESULT CoreAudioUtil::GetPreferredAudioParameters(const std::string& device_id,
                                                   bool is_output_device,
                                                   AudioParameters* params) {
  DVLOG(1) << __FUNCTION__;
  UMALogCallback uma_log_cb(
      is_output_device ? base::BindRepeating(&LogUMAPreferredOutputParams)
                       : base::BindRepeating(&LogUMAEmptyCb));
  ComPtr<IMMDevice> device(
      CreateDeviceByID(device_id, is_output_device, uma_log_cb));
  if (!device.Get())
    return E_FAIL;

  ComPtr<IAudioClient> client(CreateClientInternal(device.Get(), uma_log_cb));
  if (!client.Get())
    return E_FAIL;

  HRESULT hr = GetPreferredAudioParametersInternal(
      client.Get(), is_output_device, params, uma_log_cb);
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
    params->Reset(params->format(), CHANNEL_LAYOUT_STEREO,
                  params->sample_rate(), params->frames_per_buffer());
  }

  return hr;
}

ChannelConfig CoreAudioUtil::GetChannelConfig(const std::string& device_id,
                                              EDataFlow data_flow) {
  ComPtr<IAudioClient> client(CreateClient(device_id, data_flow, eConsole));

  WAVEFORMATPCMEX format = {};
  if (!client.Get() || FAILED(GetSharedModeMixFormat(client.Get(), &format)))
    return 0;

  return static_cast<ChannelConfig>(format.dwChannelMask);
}

HRESULT CoreAudioUtil::SharedModeInitialize(IAudioClient* client,
                                            const WAVEFORMATPCMEX* format,
                                            HANDLE event_handle,
                                            uint32_t requested_buffer_size,
                                            uint32_t* endpoint_buffer_size,
                                            const GUID* session_guid) {
  // Use default flags (i.e, dont set AUDCLNT_STREAMFLAGS_NOPERSIST) to
  // ensure that the volume level and muting state for a rendering session
  // are persistent across system restarts. The volume level and muting
  // state for a capture session are never persistent.
  DWORD stream_flags = 0;

  // Enable event-driven streaming if a valid event handle is provided.
  // After the stream starts, the audio engine will signal the event handle
  // to notify the client each time a buffer becomes ready to process.
  // Event-driven buffering is supported for both rendering and capturing.
  // Both shared-mode and exclusive-mode streams can use event-driven buffering.
  bool use_event = (event_handle != NULL &&
                    event_handle != INVALID_HANDLE_VALUE);
  if (use_event)
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  DVLOG(2) << "stream_flags: 0x" << std::hex << stream_flags;

  HRESULT hr;
  if (requested_buffer_size > 0) {
    ComPtr<IAudioClient3> audio_client_3;
    hr = client->QueryInterface(audio_client_3.GetAddressOf());
    if (FAILED(hr)) {
      DVLOG(1) << "Failed to QueryInterface on IAudioClient3 with explicit "
                  "buffer size: "
               << std::hex << hr;
      return hr;
    }
    hr = audio_client_3->InitializeSharedAudioStream(
        stream_flags, requested_buffer_size, &(format->Format), session_guid);
    if (FAILED(hr)) {
      DVLOG(1) << "IAudioClient3::InitializeSharedAudioStream: " << std::hex
               << hr;
      return hr;
    }
  } else {
    // Initialize the shared mode client for minimal delay.
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, stream_flags, 0, 0,
                            &(format->Format), session_guid);
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
  REFERENCE_TIME  latency = 0;
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

}  // namespace media
