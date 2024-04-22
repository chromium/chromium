// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_manager_win.h"

#include <objbase.h>

#include <initguid.h>
#include <windows.h>

#include <mmsystem.h>
#include <setupapi.h>
#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/win/audio_device_listener_win.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/audio/win/device_enumeration_win.h"
#include "media/audio/win/waveout_output_win.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

// The following are defined in various DDK headers, and we (re)define them here
// to avoid adding the DDK as a chrome dependency.
#define DRV_QUERYDEVICEINTERFACE 0x80c
#define DRVM_MAPPER_PREFERRED_GET 0x2015
#define DRV_QUERYDEVICEINTERFACESIZE 0x80d
DEFINE_GUID(AM_KSCATEGORY_AUDIO,
            0x6994ad04,
            0x93ef,
            0x11d0,
            0xa3,
            0xcc,
            0x00,
            0xa0,
            0xc9,
            0x22,
            0x31,
            0x96);

namespace media {

// Maximum number of output streams that can be open simultaneously.
constexpr int kMaxOutputStreams = 50;

// Up to 8 channels can be passed to the driver.  This should work, given the
// right drivers, but graceful error handling is needed.
constexpr int kWinMaxChannels = 8;

// Buffer size to use for input and output stream when a proper size can't be
// determined from the system
constexpr int kFallbackBufferSize = 2048;

namespace {

int NumberOfWaveOutBuffers() {
  // Use the user provided buffer count if provided.
  int buffers = 0;
  std::string buffers_str(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWaveOutBuffers));
  if (base::StringToInt(buffers_str, &buffers) && buffers > 0) {
    return buffers;
  }

  return 3;
}

}  // namespace

AudioManagerWin::AudioManagerWin(std::unique_ptr<AudioThread> audio_thread,
                                 AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  // |CoreAudioUtil::IsSupported()| uses static variables to avoid doing
  // multiple initializations.  This is however not thread safe.
  // So, here we call it explicitly before we kick off the audio thread
  // or do any other work.
  CoreAudioUtil::IsSupported();

  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  // WARNING: This may be executed on the UI loop, do not add any code here
  // which loads libraries or attempts to call out into the OS.  Instead add
  // such code to the InitializeOnAudioThread() method below.

  // In case we are already on the audio thread (i.e. when running out of
  // process audio), don't post.
  if (GetTaskRunner()->BelongsToCurrentThread()) {
    this->InitializeOnAudioThread();
    return;
  }

  // Task must be posted last to avoid races from handing out "this" to the
  // audio thread. Unretained is safe since we join the audio thread before
  // destructing |this|.
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerWin::InitializeOnAudioThread,
                                base::Unretained(this)));
}

AudioManagerWin::~AudioManagerWin() = default;

void AudioManagerWin::ShutdownOnAudioThread() {
  // Prevent pending callbacks from `output_device_listener_` from being run.
  // TODO(crbug.com/40066532): Remove this call when kAudioServiceOutOfProcess
  // is removed on Windows; `weak_factory_on_audio_thread_` will be guaranteed
  // to be destroyed/invalidated on the right thread then.
  weak_factory_on_audio_thread_.InvalidateWeakPtrs();

  AudioManagerBase::ShutdownOnAudioThread();

  // Destroy AudioDeviceListenerWin instance on the audio thread because it
  // expects to be constructed and destroyed on the same thread.
  output_device_listener_.reset();
}

bool AudioManagerWin::HasAudioOutputDevices() {
  if (CoreAudioUtil::IsSupported())
    return CoreAudioUtil::NumberOfActiveDevices(eRender) > 0;

  return (::waveOutGetNumDevs() != 0);
}

bool AudioManagerWin::HasAudioInputDevices() {
  if (CoreAudioUtil::IsSupported())
    return CoreAudioUtil::NumberOfActiveDevices(eCapture) > 0;

  return (::waveInGetNumDevs() != 0);
}

void AudioManagerWin::InitializeOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Initialize should only be called once.
  CHECK(!output_device_listener_);

  // Create a WeakPtr bound to the Audio thread, which will be invalidated
  // in ShutdownOnAudioThread().
  weak_this_on_audio_thread_ = weak_factory_on_audio_thread_.GetWeakPtr();

  // AudioDeviceListenerWin must be initialized on a COM thread.
  // Despite `this` owning `output_device_listener_`, we need to bind the
  // callback to a WeakPtr: NotifyAllOutputDeviceChangeListeners() will be
  // posted to the audio thread instead of being run synchronously, since we use
  // BindPostTaskToCurrentDefault().
  output_device_listener_ = std::make_unique<AudioDeviceListenerWin>(
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &AudioManagerWin::NotifyAllOutputDeviceChangeListeners,
          weak_this_on_audio_thread_)));
}

void AudioManagerWin::GetAudioDeviceNamesImpl(bool input,
                                              AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  // Enumerate all active audio-endpoint capture devices.
  if (input)
    GetInputDeviceNamesWin(device_names);
  else
    GetOutputDeviceNamesWin(device_names);

  if (!device_names->empty()) {
    device_names->push_front(AudioDeviceName::CreateCommunications());

    // Always add default device parameters as first element.
    device_names->push_front(AudioDeviceName::CreateDefault());
  }
}

void AudioManagerWin::GetAudioInputDeviceNames(AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(true, device_names);
}

void AudioManagerWin::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(false, device_names);
}

AudioParameters AudioManagerWin::GetInputStreamParameters(
    const std::string& device_id) {
  AudioParameters parameters;
  HRESULT hr =
      CoreAudioUtil::GetPreferredAudioParameters(device_id, false, &parameters);

  if (FAILED(hr) || !parameters.IsValid()) {
    LOG(WARNING) << "Unable to get preferred audio params for " << device_id
                 << " 0x" << std::hex << hr;
    // TODO(tommi): We appear to have callers to GetInputStreamParameters that
    // rely on getting valid audio parameters returned for an invalid or
    // unavailable device. We should track down those code paths (it is likely
    // that they actually don't need a real device but depend on the audio
    // code path somehow for a configuration - e.g. tab capture).
    parameters = AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                 ChannelLayoutConfig::Stereo(), 48000,
                                 kFallbackBufferSize);
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    parameters.set_frames_per_buffer(user_buffer_size);

  return parameters;
}

std::string AudioManagerWin::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  return CoreAudioUtil::GetMatchingOutputDeviceID(input_device_id);
}

const char* AudioManagerWin::GetName() {
  return "Windows";
}

// Factory for the implementations of AudioOutputStream for AUDIO_PCM_LINEAR
// mode.
// - PCMWaveOutAudioOutputStream: Based on the waveOut API.
AudioOutputStream* AudioManagerWin::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  if (params.channels() > kWinMaxChannels)
    return nullptr;

  return new PCMWaveOutAudioOutputStream(this, params, NumberOfWaveOutBuffers(),
                                         WAVE_MAPPER);
}

AudioOutputStream* AudioManagerWin::MakeBitstreamOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  return MakeLowLatencyOutputStream(params, device_id, log_callback);
#else   // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  return nullptr;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
}

// Factory for the implementations of AudioOutputStream for
// AUDIO_PCM_LOW_LATENCY mode. Two implementations should suffice most
// windows user's needs.
// - PCMWaveOutAudioOutputStream: Based on the waveOut API.
// - WASAPIAudioOutputStream: Based on Core Audio (WASAPI) API.
AudioOutputStream* AudioManagerWin::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  DCHECK(params.format() == AudioParameters::AUDIO_BITSTREAM_DTS ||
         params.format() == AudioParameters::AUDIO_PCM_LOW_LATENCY)
      << params.format();
#else
  DCHECK_EQ(params.format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
#endif

  if (params.channels() > kWinMaxChannels)
    return nullptr;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceWaveAudio)) {
    DLOG(WARNING) << "Forcing usage of Windows WaveXxx APIs";
    return nullptr;
  }

  // Pass an empty string to indicate that we want the default device
  // since we consistently only check for an empty string in
  // WASAPIAudioOutputStream.
  bool communications =
      device_id == AudioDeviceDescription::kCommunicationsDeviceId;
  return new WASAPIAudioOutputStream(
      this,
      communications || device_id == AudioDeviceDescription::kDefaultDeviceId
          ? std::string()
          : device_id,
      params, communications ? eCommunications : eConsole, log_callback);
}

// Factory for the implementations of AudioInputStream for AUDIO_PCM_LINEAR
// mode.
AudioInputStream* AudioManagerWin::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeLowLatencyInputStream(params, device_id, log_callback);
}

// Factory for the implementations of AudioInputStream for
// AUDIO_PCM_LOW_LATENCY mode.
AudioInputStream* AudioManagerWin::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  // Used for both AUDIO_PCM_LOW_LATENCY and AUDIO_PCM_LINEAR.
  return new WASAPIAudioInputStream(this, params, device_id, log_callback);
}

std::string AudioManagerWin::GetDefaultInputDeviceID() {
  return CoreAudioUtil::GetDefaultInputDeviceID();
}

std::string AudioManagerWin::GetDefaultOutputDeviceID() {
  return CoreAudioUtil::GetDefaultOutputDeviceID();
}

std::string AudioManagerWin::GetCommunicationsInputDeviceID() {
  return CoreAudioUtil::GetCommunicationsInputDeviceID();
}

std::string AudioManagerWin::GetCommunicationsOutputDeviceID() {
  return CoreAudioUtil::GetCommunicationsOutputDeviceID();
}

AudioParameters AudioManagerWin::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = 48000;
  int buffer_size = kFallbackBufferSize;
  int effects = AudioParameters::NO_EFFECTS;
  int min_buffer_size = 0;
  int max_buffer_size = 0;
  int default_buffer_size = 0;
  bool attempt_audio_offload = CoreAudioUtil::IsAudioOffloadSupported(nullptr);

  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio)) {
    // TODO(rtoy): tune these values for best possible WebAudio
    // performance. WebRTC works well at 48kHz and a buffer size of 480
    // samples will be used for this case. Note that exclusive mode is
    // experimental. This sample rate will be combined with a buffer size of
    // 256 samples, which corresponds to an output delay of ~5.33ms.
    sample_rate = 48000;
    buffer_size = 256;
    if (input_params.IsValid())
      channel_layout_config = input_params.channel_layout_config();
  } else {
    AudioParameters params;

    HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
        output_device_id.empty() ? GetDefaultOutputDeviceID()
                                 : output_device_id,
        true, &params, attempt_audio_offload);
    if (FAILED(hr)) {
      // This can happen when CoreAudio isn't supported or available
      // (e.g. certain installations of Windows Server 2008 R2).
      // Instead of returning the input_params, we'll return invalid
      // AudioParameters to make sure that an attempt to create this output
      // stream, won't succeed. This behavior is also consistent with
      // GetInputStreamParameters.
      DLOG(ERROR) << "GetPreferredAudioParameters failed: " << std::hex << hr;
      return AudioParameters();
    }
    DVLOG(1) << params.AsHumanReadableString();
    DCHECK(params.IsValid());

    channel_layout_config = params.channel_layout_config();
    buffer_size = params.frames_per_buffer();
    sample_rate = params.sample_rate();
    effects = params.effects();

    AudioParameters::HardwareCapabilities hardware_capabilities =
        params.hardware_capabilities().value_or(
            AudioParameters::HardwareCapabilities());
    min_buffer_size = hardware_capabilities.min_frames_per_buffer;
    max_buffer_size = hardware_capabilities.max_frames_per_buffer;
    default_buffer_size = hardware_capabilities.default_frames_per_buffer;
  }

  if (input_params.IsValid()) {
    // If the user has enabled checking supported channel layouts or we don't
    // have a valid channel layout yet, try to use the input layout.  See bugs
    // http://crbug.com/259165 and http://crbug.com/311906 for more details.
    if (cmd_line->HasSwitch(switches::kTrySupportedChannelLayouts) ||
        channel_layout_config.channel_layout() == CHANNEL_LAYOUT_UNSUPPORTED) {
      // Check if it is possible to open up at the specified input channel
      // layout but avoid checking if the specified layout is the same as the
      // hardware (preferred) layout. We do this extra check to avoid the
      // CoreAudioUtil::IsChannelLayoutSupported() overhead in most cases.
      if (input_params.channel_layout() !=
          channel_layout_config.channel_layout()) {
        // TODO(henrika): Internally, IsChannelLayoutSupported does many of the
        // operations that have already been done such as opening up a client
        // and fetching the WAVEFORMATPCMEX format.  Ideally we should only do
        // that once.  Then here, we can check the layout from the data we
        // already hold.
        if (CoreAudioUtil::IsChannelLayoutSupported(
                output_device_id, eRender, eConsole,
                input_params.channel_layout())) {
          // Open up using the same channel layout as the source if it is
          // supported by the hardware.
          channel_layout_config = input_params.channel_layout_config();
          DVLOG(1) << "Hardware channel layout is not used; using same layout"
                   << " as the source instead ("
                   << channel_layout_config.channel_layout() << ")";
        }
      }
    }

    effects |= input_params.effects();

    // Allow non-default buffer sizes if we have a valid min and max.
    if (min_buffer_size > 0 && max_buffer_size > 0) {
      buffer_size =
          std::min(max_buffer_size,
                   std::max(input_params.frames_per_buffer(), min_buffer_size));
    }
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  AudioParameters::HardwareCapabilities hardware_capabilities(
      min_buffer_size, max_buffer_size, default_buffer_size,
      attempt_audio_offload);
#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
  hardware_capabilities.bitstream_formats = 0;
  hardware_capabilities.require_encapsulation = false;
  if (WASAPIAudioOutputStream::GetShareMode() == AUDCLNT_SHAREMODE_EXCLUSIVE) {
    hardware_capabilities.bitstream_formats = GetPassthroughAudioFormats();
    hardware_capabilities.require_encapsulation = true;
  }
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size,
                         hardware_capabilities);
  params.set_effects(effects);
  DCHECK(params.IsValid());
  return params;
}

// static
std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerWin>(std::move(audio_thread),
                                           audio_log_factory);
}

}  // namespace media
