// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/win/audio_low_latency_input_win.h"

#include <objbase.h>

#include <propkey.h>
#include <windows.devices.enumeration.h>
#include <windows.media.devices.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/scoped_variant.h"
#include "base/win/vector.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Media::Devices::IMediaDeviceStatics;
using ABI::Windows::Media::Effects::IAudioCaptureEffectsManager;
using ABI::Windows::Media::Effects::IAudioEffectsManagerStatics;
using base::win::GetActivationFactory;
using base::win::ScopedCoMem;
using base::win::ScopedCOMInitializer;
using base::win::ScopedHString;
using Microsoft::WRL::ComPtr;

namespace media {

namespace {

constexpr char kUwpDeviceIdPrefix[] = "\\\\?\\SWD#MMDEVAPI#";

constexpr uint32_t KSAUDIO_SPEAKER_UNSUPPORTED = 0;

// Max allowed absolute difference between a QPC-based timestamp and a default
// base::TimeTicks::Now() timestamp before switching to fake audio timestamps.
constexpr base::TimeDelta kMaxAbsTimeDiffBeforeSwithingToFakeTimestamps =
    base::Milliseconds(500);

// Converts a COM error into a human-readable string.
std::string ErrorToString(HRESULT hresult) {
  return CoreAudioUtil::ErrorToString(hresult);
}

// Errors when initializing the audio client related to the audio format. Split
// by whether we're using format conversion or not. Used for reporting stats -
// do not renumber entries.
enum FormatRelatedInitError {
  kUnsupportedFormat = 0,
  kUnsupportedFormatWithFormatConversion = 1,
  kInvalidArgument = 2,
  kInvalidArgumentWithFormatConversion = 3,
  kCount
};

bool IsSupportedFormatForConversion(WAVEFORMATEXTENSIBLE* format_ex) {
  WAVEFORMATEX* format = &format_ex->Format;
  if (format->nSamplesPerSec < limits::kMinSampleRate ||
      format->nSamplesPerSec > limits::kMaxSampleRate) {
    return false;
  }

  switch (format->wBitsPerSample) {
    case 8:
    case 16:
    case 32:
      break;
    default:
      return false;
  }

  if (GuessChannelLayout(format->nChannels) == CHANNEL_LAYOUT_UNSUPPORTED) {
    LOG(ERROR) << "Hardware configuration not supported for audio conversion";
    return false;
  }

  return true;
}

// Converts ChannelLayout to Microsoft's channel configuration but only discrete
// and up to stereo is supported currently. All other multi-channel layouts
// return KSAUDIO_SPEAKER_UNSUPPORTED.
ChannelConfig ChannelLayoutToChannelConfig(ChannelLayout layout) {
  switch (layout) {
    case CHANNEL_LAYOUT_DISCRETE:
      return KSAUDIO_SPEAKER_DIRECTOUT;
    case CHANNEL_LAYOUT_MONO:
      return KSAUDIO_SPEAKER_MONO;
    case CHANNEL_LAYOUT_STEREO:
      return KSAUDIO_SPEAKER_STEREO;
    default:
      LOG(WARNING) << "Unsupported channel layout: " << layout;
      // KSAUDIO_SPEAKER_UNSUPPORTED equals 0 and corresponds to "no specific
      // channel order".
      return KSAUDIO_SPEAKER_UNSUPPORTED;
  }
}

const char* StreamOpenResultToString(
    WASAPIAudioInputStream::StreamOpenResult result) {
  switch (result) {
    case WASAPIAudioInputStream::OPEN_RESULT_OK:
      return "OK";
    case WASAPIAudioInputStream::OPEN_RESULT_CREATE_INSTANCE:
      return "CREATE_INSTANCE";
    case WASAPIAudioInputStream::OPEN_RESULT_NO_ENDPOINT:
      return "NO_ENDPOINT";
    case WASAPIAudioInputStream::OPEN_RESULT_NO_STATE:
      return "NO_STATE";
    case WASAPIAudioInputStream::OPEN_RESULT_DEVICE_NOT_ACTIVE:
      return "DEVICE_NOT_ACTIVE";
    case WASAPIAudioInputStream::OPEN_RESULT_ACTIVATION_FAILED:
      return "ACTIVATION_FAILED";
    case WASAPIAudioInputStream::OPEN_RESULT_FORMAT_NOT_SUPPORTED:
      return "FORMAT_NOT_SUPPORTED";
    case WASAPIAudioInputStream::OPEN_RESULT_AUDIO_CLIENT_INIT_FAILED:
      return "AUDIO_CLIENT_INIT_FAILED";
    case WASAPIAudioInputStream::OPEN_RESULT_GET_BUFFER_SIZE_FAILED:
      return "GET_BUFFER_SIZE_FAILED";
    case WASAPIAudioInputStream::OPEN_RESULT_LOOPBACK_ACTIVATE_FAILED:
      return "LOOPBACK_ACTIVATE_FAILED";
    case WASAPIAudioInputStream::OPEN_RESULT_LOOPBACK_INIT_FAILED:
      return "LOOPBACK_INIT_FAILED";
    case WASAPIAudioInputStream::OPEN_RESULT_SET_EVENT_HANDLE:
      return "SET_EVENT_HANDLE";
    case WASAPIAudioInputStream::OPEN_RESULT_NO_CAPTURE_CLIENT:
      return "NO_CAPTURE_CLIENT";
    case WASAPIAudioInputStream::OPEN_RESULT_NO_AUDIO_VOLUME:
      return "NO_AUDIO_VOLUME";
    case WASAPIAudioInputStream::OPEN_RESULT_OK_WITH_RESAMPLING:
      return "OK_WITH_RESAMPLING";
  }
  return "UNKNOWN";
}

const char* EffectTypeToString(
    ABI::Windows::Media::Effects::AudioEffectType type) {
  switch (type) {
    case ABI::Windows::Media::Effects::AudioEffectType_Other:
      return "Other/None";
    case ABI::Windows::Media::Effects::AudioEffectType_AcousticEchoCancellation:
      return "AcousticEchoCancellation";
    case ABI::Windows::Media::Effects::AudioEffectType_NoiseSuppression:
      return "NoiseSuppression";
    case ABI::Windows::Media::Effects::AudioEffectType_AutomaticGainControl:
      return "AutomaticGainControl";
    case ABI::Windows::Media::Effects::AudioEffectType_BeamForming:
      return "BeamForming";
    case ABI::Windows::Media::Effects::AudioEffectType_ConstantToneRemoval:
      return "ConstantToneRemoval";
    case ABI::Windows::Media::Effects::AudioEffectType_Equalizer:
      return "Equalizer";
    case ABI::Windows::Media::Effects::AudioEffectType_LoudnessEqualizer:
      return "LoudnessEqualizer";
    case ABI::Windows::Media::Effects::AudioEffectType_BassBoost:
      return "BassBoost";
    case ABI::Windows::Media::Effects::AudioEffectType_VirtualSurround:
      return "VirtualSurround";
    case ABI::Windows::Media::Effects::AudioEffectType_VirtualHeadphones:
      return "VirtualHeadphones";
    case ABI::Windows::Media::Effects::AudioEffectType_SpeakerFill:
      return "SpeakerFill";
    case ABI::Windows::Media::Effects::AudioEffectType_RoomCorrection:
      return "RoomCorrection";
    case ABI::Windows::Media::Effects::AudioEffectType_BassManagement:
      return "BassManagement";
    case ABI::Windows::Media::Effects::AudioEffectType_EnvironmentalEffects:
      return "EnvironmentalEffects";
    case ABI::Windows::Media::Effects::AudioEffectType_SpeakerProtection:
      return "SpeakerProtection";
    case ABI::Windows::Media::Effects::AudioEffectType_SpeakerCompensation:
      return "SpeakerCompensation";
    case ABI::Windows::Media::Effects::AudioEffectType_DynamicRangeCompression:
      return "DynamicRangeCompression";
    case ABI::Windows::Media::Effects::AudioEffectType_FarFieldBeamForming:
      return "FarFieldBeamForming";
    case ABI::Windows::Media::Effects::AudioEffectType_DeepNoiseSuppression:
      return "DeepNoiseSuppression";
  }
  return "Unknown";
}

bool VariantBoolToBool(VARIANT_BOOL var_bool) {
  switch (var_bool) {
    case VARIANT_TRUE:
      return true;
    case VARIANT_FALSE:
      return false;
  }
  LOG(ERROR) << "Invalid VARIANT_BOOL type";
  return false;
}

std::string GetOpenLogString(WASAPIAudioInputStream::StreamOpenResult result,
                             HRESULT hr,
                             WAVEFORMATEXTENSIBLE input_format,
                             WAVEFORMATEX output_format) {
  return base::StringPrintf(
      "WAIS::Open => (ERROR: result=%s, hresult=%#lx, input_format=[%s], "
      "output_format=[%s])",
      StreamOpenResultToString(result), hr,
      CoreAudioUtil::WaveFormatToString(&input_format).c_str(),
      CoreAudioUtil::WaveFormatToString(&output_format).c_str());
}

bool InitializeUWPSupport() {
  // Place the actual body of the initialization in a lambda and store the
  // result as a static since we don't expect this result to change between
  // runs.
  static const bool initialization_result = []() {
    // Windows.Media.Effects and Windows.Media.Devices requires Windows 10 build
    // 10.0.10240.0.
    DCHECK_GE(base::win::OSInfo::GetInstance()->version_number().build, 10240u);

    return true;
  }();

  return initialization_result;
}

void LogFakeAudioCaptureTimestamps(bool use_fake_audio_capture_timestamps,
                                   base::TimeDelta abs_delta_time) {
  TRACE_EVENT_INSTANT2(
      "audio", "AudioCaptureWinTimestamps", TRACE_EVENT_SCOPE_THREAD,
      "use_fake_audio_capture_timestamps", use_fake_audio_capture_timestamps,
      "abs_timestamp_diff_ms", abs_delta_time.InMilliseconds());
  base::UmaHistogramBoolean("Media.Audio.Capture.Win.FakeTimestamps",
                            use_fake_audio_capture_timestamps);
  base::UmaHistogramLongTimes("Media.Audio.Capture.Win.AbsTimestampDiffMs",
                              abs_delta_time);
}

}  // namespace

// Counts how often an OS capture callback reports a data discontinuity and logs
// it as a UMA histogram.
class WASAPIAudioInputStream::DataDiscontinuityReporter {
 public:
  // Logs once every 10s, assuming 10ms buffers.
  constexpr static int kCallbacksPerLogPeriod = 1000;

  DataDiscontinuityReporter() {}

  int GetLongTermDiscontinuityCountAndReset() {
    int long_term_count = data_discontinuity_long_term_count_;
    callback_count_ = 0;
    data_discontinuity_short_term_count_ = 0;
    data_discontinuity_long_term_count_ = 0;
    return long_term_count;
  }

  void Log(bool observed_data_discontinuity) {
    ++callback_count_;
    if (observed_data_discontinuity) {
      ++data_discontinuity_short_term_count_;
      ++data_discontinuity_long_term_count_;
    }

    if (callback_count_ % kCallbacksPerLogPeriod)
      return;

    // TODO(crbug.com/41378888): It can be possible to replace
    // "Media.Audio.Capture.Glitches2" with this new (simplified) metric
    // instead.
    base::UmaHistogramCounts1000("Media.Audio.Capture.Win.Glitches2",
                                 data_discontinuity_short_term_count_);

    data_discontinuity_short_term_count_ = 0;
  }

 private:
  int callback_count_ = 0;
  int data_discontinuity_short_term_count_ = 0;
  int data_discontinuity_long_term_count_ = 0;
};

WASAPIAudioInputStream::WASAPIAudioInputStream(
    AudioManagerWin* manager,
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback)
    : manager_(manager),
      glitch_reporter_(SystemGlitchReporter::StreamType::kCapture),
      peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(manager_),
                                         /*trace_start=*/true)),
      data_discontinuity_reporter_(
          std::make_unique<DataDiscontinuityReporter>()),
      device_id_(device_id),
      log_callback_(std::move(log_callback)) {
  DCHECK(manager_);
  DCHECK(!device_id_.empty());
  DCHECK(!log_callback_.is_null());
  DCHECK_LE(params.channels(), 2);
  DCHECK(params.channel_layout() == CHANNEL_LAYOUT_MONO ||
         params.channel_layout() == CHANNEL_LAYOUT_STEREO ||
         params.channel_layout() == CHANNEL_LAYOUT_DISCRETE);
  SendLogMessage("%s({device_id=%s}, {params=[%s]})", __func__,
                 device_id.c_str(), params.AsHumanReadableString().c_str());

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  if (!avrt_init)
    SendLogMessage("%s => (WARNING: failed to load Avrt.dll)", __func__);

  const SampleFormat kSampleFormat = kSampleFormatS16;

  // The clients asks for an input stream specified by |params|. Start by
  // setting up an input device format according to the same specification.
  // If all goes well during the upcoming initialization, this format will not
  // change. However, under some circumstances, minor changes can be required
  // to fit the current input audio device. If so, a FIFO and/or and audio
  // converter might be needed to ensure that the output format of this stream
  // matches what the client asks for.
  WAVEFORMATEX* format = &input_format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = params.channels();
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = SampleFormatToBitsPerChannel(kSampleFormat);
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;

  // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE which can be
  // required in combination with e.g. multi-channel microphone arrays.
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  input_format_.Samples.wValidBitsPerSample = format->wBitsPerSample;
  input_format_.dwChannelMask =
      ChannelLayoutToChannelConfig(params.channel_layout());
  input_format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  SendLogMessage("%s => (audio engine format=[%s])", __func__,
                 CoreAudioUtil::WaveFormatToString(&input_format_).c_str());

  // Set up the fixed output format based on |params|. Will not be changed and
  // does not required an extended wave format structure since any multi-channel
  // input will be converted to stereo.
  output_format_.wFormatTag = WAVE_FORMAT_PCM;
  output_format_.nChannels = format->nChannels;
  output_format_.nSamplesPerSec = format->nSamplesPerSec;
  output_format_.wBitsPerSample = format->wBitsPerSample;
  output_format_.nBlockAlign = format->nBlockAlign;
  output_format_.nAvgBytesPerSec = format->nAvgBytesPerSec;
  output_format_.cbSize = 0;
  SendLogMessage("%s => (audio sink format=[%s])", __func__,
                 CoreAudioUtil::WaveFormatToString(&output_format_).c_str());

  // Size in bytes of each audio frame.
  frame_size_bytes_ = format->nBlockAlign;

  // Store size of audio packets which we expect to get from the audio
  // endpoint device in each capture event.
  packet_size_bytes_ = params.GetBytesPerBuffer(kSampleFormat);
  packet_size_frames_ = packet_size_bytes_ / format->nBlockAlign;
  SendLogMessage(
      "%s => (packet size=[%zu bytes/%zu audio frames/%.3f milliseconds])",
      __func__, packet_size_bytes_, packet_size_frames_,
      params.GetBufferDuration().InMillisecondsF());

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_ready_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_ready_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_capture_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_capture_event_.IsValid());
}

WASAPIAudioInputStream::~WASAPIAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AudioInputStream::OpenOutcome WASAPIAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendLogMessage("%s([opened=%s])", __func__, opened_ ? "true" : "false");
  if (opened_) {
    return OpenOutcome::kAlreadyOpen;
  }

  // Obtain a reference to the IMMDevice interface of the capturing device with
  // the specified unique identifier or role which was set at construction.
  HRESULT hr = SetCaptureDevice();
  if (FAILED(hr)) {
    ReportOpenResult(hr);
    return OpenOutcome::kFailed;
  }

  // Check if raw audio processing is supported for the selected capture device.
  raw_processing_supported_ = RawProcessingSupported();

  if (raw_processing_supported_ &&
      !AudioDeviceDescription::IsLoopbackDevice(device_id_) &&
      InitializeUWPSupport()) {
    // Retrieve a unique identifier of the selected audio device but in a
    // format which can be used by UWP (or Core WinRT) APIs. It can then be
    // utilized in combination with the Windows.Media.Effects UWP API to
    // discover the audio processing chain on a device.
    std::string uwp_device_id = GetUWPDeviceId();
    if (!uwp_device_id.empty()) {
      // For the selected device, generate two lists of enabled audio effects
      // and store them in |default_effect_types_| and |raw_effect_types_|.
      // Default corresponds to "Normal audio signal processing" and Raw is for
      // "Minimal audio signal processing". These two lists are used for UMA
      // stats when the stream is closed.
      GetAudioCaptureEffects(uwp_device_id);
    }
  }

  // Obtain an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  hr = endpoint_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  &audio_client_);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_ACTIVATION_FAILED;
    ReportOpenResult(hr);
    return OpenOutcome::kFailed;
  }

  // Raw audio capture suppresses processing that down mixes e.g. a microphone
  // array into a supported format and instead exposes the device's native
  // format. Chrome only supports a maximum number of input channels given by
  // media::kMaxConcurrentChannels. Therefore, one additional test is needed
  // before stating that raw audio processing can be supported.
  // Failure will not prevent opening but the method must succeed to be able to
  // select raw input capture mode.
  WORD audio_engine_channels = 0;
  hr = GetAudioEngineNumChannels(&audio_engine_channels);

  // Attempt to enable communications category and raw capture mode on the audio
  // stream. Ignoring return value since the method logs its own error messages
  // and it should be OK to continue opening the stream even after a failure.
  if (raw_processing_supported_ &&
      !AudioDeviceDescription::IsLoopbackDevice(device_id_) && SUCCEEDED(hr)) {
    SetCommunicationsCategoryAndMaybeRawCaptureMode(audio_engine_channels);
  }

  // Verify that the selected audio endpoint supports the specified format
  // set during construction and using the specified client properties.
  hr = S_OK;
  if (!DesiredFormatIsSupported(&hr)) {
    open_result_ = OPEN_RESULT_FORMAT_NOT_SUPPORTED;
    ReportOpenResult(hr);
    return OpenOutcome::kFailed;
  }

  // Initialize the audio stream between the client and the device using
  // shared mode and a lowest possible glitch-free latency.
  hr = InitializeAudioEngine();
  if (SUCCEEDED(hr) && converter_)
    open_result_ = OPEN_RESULT_OK_WITH_RESAMPLING;
  ReportOpenResult(hr);  // Report before we assign a value to |opened_|.
  opened_ = SUCCEEDED(hr);

  if (opened_) {
    return OpenOutcome::kSuccess;
  }

  switch (hr) {
    case E_ACCESSDENIED:
      return OpenOutcome::kFailedSystemPermissions;
    case AUDCLNT_E_DEVICE_IN_USE:
      return OpenOutcome::kFailedInUse;
    default:
      return OpenOutcome::kFailed;
  }
}

void WASAPIAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  SendLogMessage("%s([opened=%s, started=%s])", __func__,
                 opened_ ? "true" : "false", started_ ? "true" : "false");
  if (!opened_)
    return;

  if (started_)
    return;

  if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId &&
      system_audio_volume_) {
    BOOL muted = false;
    system_audio_volume_->GetMute(&muted);

    // If the system audio is muted at the time of capturing, then no need to
    // mute it again, and later we do not unmute system audio when stopping
    // capturing.
    if (!muted) {
      system_audio_volume_->SetMute(true, nullptr);
      mute_done_ = true;
    }
  }

  DCHECK(!sink_);
  sink_ = callback;

  // Starts periodic AGC microphone measurements if the AGC has been enabled
  // using SetAutomaticGainControl().
  StartAgc();

  // Create and start the thread that will drive the capturing by waiting for
  // capture events.
  DCHECK(!capture_thread_.get());
  capture_thread_ = std::make_unique<base::DelegateSimpleThread>(
      this, "wasapi_capture_thread",
      base::SimpleThread::Options(base::ThreadType::kRealtimeAudio));
  capture_thread_->Start();

  // Start streaming data between the endpoint buffer and the audio engine.
  HRESULT hr = audio_client_->Start();
  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: IAudioClient::Start=[%s])", __func__,
                   ErrorToString(hr).c_str());
  }

  if (SUCCEEDED(hr) && audio_render_client_for_loopback_.Get()) {
    hr = audio_render_client_for_loopback_->Start();
    if (FAILED(hr))
      SendLogMessage("%s => (ERROR: IAudioClient::Start=[%s] (loopback))",
                     __func__, ErrorToString(hr).c_str());
  }

  started_ = SUCCEEDED(hr);
}

void WASAPIAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendLogMessage("%s([started=%s])", __func__, started_ ? "true" : "false");
  if (!started_)
    return;

  // We have muted system audio for capturing, so we need to unmute it when
  // capturing stops.
  if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId &&
      mute_done_) {
    DCHECK(system_audio_volume_);
    if (system_audio_volume_) {
      system_audio_volume_->SetMute(false, nullptr);
      mute_done_ = false;
    }
  }

  // Stops periodic AGC microphone measurements.
  StopAgc();

  // Shut down the capture thread.
  if (stop_capture_event_.IsValid()) {
    SetEvent(stop_capture_event_.Get());
  }

  // Stop the input audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: IAudioClient::Stop=[%s])", __func__,
                   ErrorToString(hr).c_str());
  }

  // Wait until the thread completes and perform cleanup.
  if (capture_thread_) {
    SetEvent(stop_capture_event_.Get());
    capture_thread_->Join();
    capture_thread_.reset();
  }

  SendLogMessage(
      "%s => (timestamp(n)-timestamp(n-1)=[min: %.3f msec, max: %.3f msec])",
      __func__, min_timestamp_diff_.InMillisecondsF(),
      max_timestamp_diff_.InMillisecondsF());

  started_ = false;
  sink_ = nullptr;
}

void WASAPIAudioInputStream::Close() {
  SendLogMessage("%s()", __func__);
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  // Only upload UMA histogram for the case when AGC is enabled, i.e., for
  // WebRTC based audio input streams.
  if (GetAutomaticGainControl()) {
    // Upload UMA histogram to track if the capture device supported raw audio
    // capture or not. See https://crbug.com/1133643.
    base::UmaHistogramBoolean("Media.Audio.RawProcessingSupportedWin",
                              raw_processing_supported_);

    // These UMAs are deprecated but keep adding the information as text logs
    // for debugging purposes.
    for (auto const& type : default_effect_types_) {
      SendLogMessage("%s => (Media.Audio.Capture.Win.DefaultEffectType=%s)",
                     __func__, EffectTypeToString(type));
    }
    for (auto const& type : raw_effect_types_) {
      SendLogMessage("%s => (Media.Audio.Capture.Win.RawEffectType=%s)",
                     __func__, EffectTypeToString(type));
    }
  }

  if (converter_)
    converter_->RemoveInput(this);

  ReportAndResetGlitchStats();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

double WASAPIAudioInputStream::GetMaxVolume() {
  // Verify that Open() has been called successfully, to ensure that an audio
  // session exists and that an ISimpleAudioVolume interface has been created.
  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return 0.0;

  // The effective volume value is always in the range 0.0 to 1.0, hence
  // we can return a fixed value (=1.0) here.
  return 1.0;
}

void WASAPIAudioInputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(volume, 0.0);
  DCHECK_LE(volume, 1.0);
  SendLogMessage("%s({volume=%.2f} [opened=%s])", __func__, volume,
                 opened_ ? "true" : "false");
  if (!opened_)
    return;

  // Set a new master volume level. Valid volume levels are in the range
  // 0.0 to 1.0. Ignore volume-change events.
  HRESULT hr = simple_audio_volume_->SetMasterVolume(static_cast<float>(volume),
                                                     nullptr);
  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: ISimpleAudioVolume::SetMasterVolume=[%s])",
                   __func__, ErrorToString(hr).c_str());
  }

  // Update the AGC volume level based on the last setting above. Note that,
  // the volume-level resolution is not infinite and it is therefore not
  // possible to assume that the volume provided as input parameter can be
  // used directly. Instead, a new query to the audio hardware is required.
  // This method does nothing if AGC is disabled.
  UpdateAgcVolume();
}

double WASAPIAudioInputStream::GetVolume() {
  DCHECK(opened_) << "Open() has not been called successfully";
  if (!opened_)
    return 0.0;

  // Retrieve the current volume level. The value is in the range 0.0 to 1.0.
  float level = 0.0f;
  HRESULT hr = simple_audio_volume_->GetMasterVolume(&level);
  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: ISimpleAudioVolume::GetMasterVolume=[%s])",
                   __func__, ErrorToString(hr).c_str());
  }

  return static_cast<double>(level);
}

bool WASAPIAudioInputStream::IsMuted() {
  DCHECK(opened_) << "Open() has not been called successfully";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!opened_)
    return false;

  // Retrieves the current muting state for the audio session.
  BOOL is_muted = FALSE;
  HRESULT hr = simple_audio_volume_->GetMute(&is_muted);
  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: ISimpleAudioVolume::GetMute=[%s])", __func__,
                   ErrorToString(hr).c_str());
  }

  return is_muted != FALSE;
}

void WASAPIAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

void WASAPIAudioInputStream::SendLogMessage(const char* format, ...) {
  if (log_callback_.is_null())
    return;
  va_list args;
  va_start(args, format);
  std::string msg("WAIS::" + base::StringPrintV(format, args));
  log_callback_.Run(msg);
  va_end(args);
}

void WASAPIAudioInputStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  DWORD task_index = 0;
  HANDLE mm_task =
      avrt::AvSetMmThreadCharacteristics(L"Pro Audio", &task_index);
  bool mmcss_is_ok =
      (mm_task && avrt::AvSetMmThreadPriority(mm_task, AVRT_PRIORITY_CRITICAL));
  if (!mmcss_is_ok) {
    // Failed to enable MMCSS on this thread. It is not fatal but can lead
    // to reduced QoS at high load.
    DWORD err = GetLastError();
    LOG(ERROR) << "WAIS::" << __func__
               << " => (ERROR: Failed to enable MMCSS (error code=" << err
               << "))";
  }

  // Allocate a buffer with a size that enables us to take care of cases like:
  // 1) The recorded buffer size is smaller, or does not match exactly with,
  //    the selected packet size used in each callback.
  // 2) The selected buffer size is larger than the recorded buffer size in
  //    each event.
  // In the case where no resampling is required, a single buffer should be
  // enough but in case we get buffers that don't match exactly, we'll go with
  // two. Same applies if we need to resample and the buffer ratio is perfect.
  // However if the buffer ratio is imperfect, we will need 3 buffers to safely
  // be able to buffer up data in cases where a conversion requires two audio
  // buffers (and we need to be able to write to the third one).
  size_t capture_buffer_size =
      std::max(2 * endpoint_buffer_size_frames_ * frame_size_bytes_,
               2 * packet_size_frames_ * frame_size_bytes_);
  int buffers_required = capture_buffer_size / packet_size_bytes_;
  if (converter_ && imperfect_buffer_size_conversion_)
    ++buffers_required;

  DCHECK(!fifo_);
  fifo_ = std::make_unique<AudioBlockFifo>(
      input_format_.Format.nChannels, packet_size_frames_, buffers_required);
  DVLOG(1) << "AudioBlockFifo buffer count: " << buffers_required;

  bool recording = true;
  bool error = false;
  HANDLE wait_array[2] = {stop_capture_event_.Get(),
                          audio_samples_ready_event_.Get()};

  record_start_time_ = base::TimeTicks::Now();
  last_capture_time_ = base::TimeTicks();
  max_timestamp_diff_ = base::TimeDelta::Min();
  min_timestamp_diff_ = base::TimeDelta::Max();

  while (recording && !error) {
    // Wait for a close-down event or a new capture event.
    DWORD wait_result = WaitForMultipleObjects(2, wait_array, FALSE, INFINITE);
    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_capture_event_| has been set.
        recording = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |audio_samples_ready_event_| has been set.
        PullCaptureDataAndPushToSink();
        break;
      case WAIT_FAILED:
      default:
        error = true;
        break;
    }
  }

  if (recording && error) {
    // TODO(henrika): perhaps it worth improving the cleanup here by e.g.
    // stopping the audio client, joining the thread etc.?
    auto saved_last_error = GetLastError();
    NOTREACHED_IN_MIGRATION()
        << "WASAPI capturing failed with error code " << saved_last_error;
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }

  fifo_.reset();
}

void WASAPIAudioInputStream::PullCaptureDataAndPushToSink() {
  TRACE_EVENT1("audio", "WASAPIAudioInputStream::PullCaptureDataAndPushToSink",
               "sample rate", input_format_.Format.nSamplesPerSec);

  UINT64 last_device_position = 0;
  UINT32 num_frames_in_next_packet = 0;

  // Get the number of frames in the next data packet in the capture endpoint
  // buffer. The count reported by GetNextPacketSize matches the count retrieved
  // in the GetBuffer call that follows this call.
  HRESULT hr =
      audio_capture_client_->GetNextPacketSize(&num_frames_in_next_packet);
  if (FAILED(hr)) {
    LOG(ERROR) << "WAIS::" << __func__
               << " => (ERROR: 1-IAudioCaptureClient::GetNextPacketSize=["
               << ErrorToString(hr).c_str() << "])";
    return;
  }

  // Pull data from the capture endpoint buffer until it's empty or an error
  // occurs. Drains the WASAPI capture buffer fully.
  while (num_frames_in_next_packet > 0) {
    BYTE* data_ptr = nullptr;
    UINT32 num_frames_to_read = 0;
    DWORD flags = 0;
    UINT64 device_position = 0;
    UINT64 capture_time_100ns = 0;

    // Retrieve the amount of data in the capture endpoint buffer, replace it
    // with silence if required, create callbacks for each packet and store
    // non-delivered data for the next event.
    hr =
        audio_capture_client_->GetBuffer(&data_ptr, &num_frames_to_read, &flags,
                                         &device_position, &capture_time_100ns);
    if (hr == AUDCLNT_S_BUFFER_EMPTY) {
      DCHECK_EQ(num_frames_to_read, 0u);
      return;
    }
    if (hr == AUDCLNT_E_OUT_OF_ORDER) {
      // A previous IAudioCaptureClient::GetBuffer() call is still in effect.
      // Release any acquired buffer to be able to try reading a buffer again.
      audio_capture_client_->ReleaseBuffer(num_frames_to_read);
    }
    if (FAILED(hr)) {
      LOG(ERROR) << "WAIS::" << __func__
                 << " => (ERROR: IAudioCaptureClient::GetBuffer=["
                 << ErrorToString(hr).c_str() << "])";
      return;
    }

    // Check if QPC-based timestamps provided by IAudioCaptureClient::GetBuffer
    // can be used for audio timestamps or not. If not, base::TimeTicks::Now()
    // will be used instead to generate the timestamps (called "fake" here). In
    // the majority of cases, fake timestamps will not be utilized and the
    // difference in `delta_time` below will be about the same size as the
    // native buffer size (e.g. 10 msec).
    // http://crbug.com/1439283 for details why this check is needed.
    if (!use_fake_audio_capture_timestamps_.has_value()) {
      base::TimeDelta delta_time =
          base::TimeTicks::Now() -
          base::TimeTicks::FromQPCValue(capture_time_100ns);
      if (delta_time.magnitude() >
          kMaxAbsTimeDiffBeforeSwithingToFakeTimestamps) {
        use_fake_audio_capture_timestamps_ = true;
        LOG(WARNING) << "WAIS::" << __func__
                     << " => (WARNING: capture timestamps will be fake)";
      } else {
        use_fake_audio_capture_timestamps_ = false;
      }
      LogFakeAudioCaptureTimestamps(use_fake_audio_capture_timestamps_.value(),
                                    delta_time.magnitude());
    }

    // The data in the packet is not correlated with the previous packet's
    // device position; this is possibly due to a stream state transition or
    // timing glitch. Note that, usage of this flag was added after the existing
    // glitch detection and it will be used as a supplementary scheme initially.
    // The behavior of the AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY flag is
    // undefined on the application's first call to GetBuffer after Start and
    // Windows 7 or later is required for support.
    // TODO(crbug.com/40261628): take this into account when reporting
    // glitch info.
    const bool observed_data_discontinuity =
        (device_position > 0 && flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY);
    if (observed_data_discontinuity) {
      LOG(WARNING) << "WAIS::" << __func__
                   << " => (WARNING: AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)";
    }
    data_discontinuity_reporter_->Log(observed_data_discontinuity);

    // The time at which the device's stream position was recorded is uncertain.
    // Thus, the client might be unable to accurately set a time stamp for the
    // current data packet.
    bool timestamp_error_was_detected = false;
    if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) {
      // TODO(crbug.com/41378888): it might be possible to improve error
      // handling here and avoid using the counter in |capture_time_100ns|.
      LOG(WARNING) << "WAIS::" << __func__
                   << " => (WARNING: AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)";
      if (num_timestamp_errors_ == 0) {
        // Measure the time it took until the first timestamp error was found.
        time_until_first_timestamp_error_ =
            base::TimeTicks::Now() - record_start_time_;
      }
      ++num_timestamp_errors_;
      timestamp_error_was_detected = true;
    }

    // If the device position has changed, we assume this data belongs to a new
    // chunk, so we report delay and glitch stats and update the last and next
    // expected device positions.
    // If the device position has not changed we assume this data belongs to the
    // previous chunk, and only update the expected next device position.
    if (device_position != last_device_position) {
      if (expected_next_device_position_ != 0) {
        base::TimeDelta glitch_duration;
        if (device_position > expected_next_device_position_) {
          glitch_duration = AudioTimestampHelper::FramesToTime(
              device_position - expected_next_device_position_,
              input_format_.Format.nSamplesPerSec);
        }
        glitch_reporter_.UpdateStats(glitch_duration);
        if (glitch_duration.is_positive()) {
          glitch_accumulator_.Add(AudioGlitchInfo::SingleBoundedSystemGlitch(
              glitch_duration, AudioGlitchInfo::Direction::kCapture));
        }
      }

      last_device_position = device_position;
      expected_next_device_position_ = device_position + num_frames_to_read;
    } else {
      expected_next_device_position_ += num_frames_to_read;
    }

    base::TimeTicks capture_time;
    if (use_fake_audio_capture_timestamps_.has_value() &&
        *use_fake_audio_capture_timestamps_) {
      capture_time = base::TimeTicks::Now();
    } else if (!timestamp_error_was_detected) {
      // Use the latest |capture_time_100ns| since it is marked as valid.
      capture_time += base::Microseconds(capture_time_100ns / 10.0);
    }
    if (capture_time <= last_capture_time_) {
      // Latest |capture_time_100ns| can't be trusted. Ensure a monotonic time-
      // stamp sequence by adding one microsecond to the latest timestamp.
      capture_time = last_capture_time_ + base::Microseconds(1);
    }

    // Keep track of max and min time difference between two successive time-
    // stamps. Results are used in Stop() to verify that the time-stamp sequence
    // was monotonic.
    if (!last_capture_time_.is_null()) {
      const auto delta_ts = capture_time - last_capture_time_;
      DCHECK_GT(device_position, 0u);
      DCHECK_GT(delta_ts, base::TimeDelta::Min());
      if (delta_ts > max_timestamp_diff_) {
        max_timestamp_diff_ = delta_ts;
      } else if (delta_ts < min_timestamp_diff_) {
        min_timestamp_diff_ = delta_ts;
      }
    }

    // Store the capture timestamp. Might be used as reference next time if
    // a new valid timestamp can't be retrieved to always guarantee a monotonic
    // sequence.
    last_capture_time_ = capture_time;

    // Adjust |capture_time| for the FIFO before pushing.
    capture_time -= AudioTimestampHelper::FramesToTime(
        fifo_->GetAvailableFrames(), input_format_.Format.nSamplesPerSec);

    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
      fifo_->PushSilence(num_frames_to_read);
    } else {
      const int bytes_per_sample = input_format_.Format.wBitsPerSample / 8;

      peak_detector_.FindPeak(data_ptr, num_frames_to_read, bytes_per_sample);
      fifo_->Push(data_ptr, num_frames_to_read, bytes_per_sample);
    }

    hr = audio_capture_client_->ReleaseBuffer(num_frames_to_read);
    if (FAILED(hr)) {
      LOG(ERROR) << "WAIS::" << __func__
                 << " => (ERROR: IAudioCaptureClient::ReleaseBuffer=["
                 << ErrorToString(hr).c_str() << "])";
      return;
    }

    TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("audio"),
                 "AudioInputCallback::OnData", "capture_time",
                 capture_time - base::TimeTicks(), "time_ticks_now",
                 base::TimeTicks::Now() - base::TimeTicks());

    // Get a cached AGC volume level which is updated once every second on the
    // audio manager thread. Note that, |volume| is also updated each time
    // SetVolume() is called through IPC by the render-side AGC.
    double volume = 0.0;
    GetAgcVolume(&volume);

    // Deliver captured data to the registered consumer using a packet size
    // which was specified at construction.
    while (fifo_->available_blocks()) {
      if (converter_) {
        if (imperfect_buffer_size_conversion_ &&
            fifo_->available_blocks() == 1) {
          // Special case. We need to buffer up more audio before we can convert
          // or else we'll suffer an underrun.
          // TODO(grunell): Verify this is really true.
          return;
        }
        converter_->Convert(convert_bus_.get());
        sink_->OnData(convert_bus_.get(), capture_time, volume,
                      glitch_accumulator_.GetAndReset());

        // Move the capture time forward for each vended block.
        capture_time += AudioTimestampHelper::FramesToTime(
            convert_bus_->frames(), output_format_.nSamplesPerSec);
      } else {
        sink_->OnData(fifo_->Consume(), capture_time, volume,
                      glitch_accumulator_.GetAndReset());

        // Move the capture time forward for each vended block.
        capture_time += AudioTimestampHelper::FramesToTime(
            packet_size_frames_, input_format_.Format.nSamplesPerSec);
      }
    }

    // Get the number of frames in the next data packet in the capture endpoint
    // buffer. Keep reading if more samples exist.
    hr = audio_capture_client_->GetNextPacketSize(&num_frames_in_next_packet);
    if (FAILED(hr)) {
      LOG(ERROR) << "WAIS::" << __func__
                 << " => (ERROR: 2-IAudioCaptureClient::GetNextPacketSize=["
                 << ErrorToString(hr).c_str() << "])";
      return;
    }
  }  // while (num_frames_in_next_packet > 0)
}

void WASAPIAudioInputStream::HandleError(HRESULT err) {
  NOTREACHED_IN_MIGRATION() << "Error code: " << err;
  if (sink_)
    sink_->OnError();
}

HRESULT WASAPIAudioInputStream::SetCaptureDevice() {
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);
  DCHECK(!endpoint_device_.Get());
  SendLogMessage("%s()", __func__);

  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_CREATE_INSTANCE;
    return hr;
  }

  // Retrieve the IMMDevice by using the specified role or the specified
  // unique endpoint device-identification string.

  // To open a stream in loopback mode, the client must obtain an IMMDevice
  // interface for the rendering endpoint device. Make that happen if needed;
  // otherwise use default capture data-flow direction.
  const EDataFlow data_flow =
      AudioDeviceDescription::IsLoopbackDevice(device_id_) ? eRender : eCapture;
  // Determine selected role to be used if the device is a default device.
  const ERole role = AudioDeviceDescription::IsCommunicationsDevice(device_id_)
                         ? eCommunications
                         : eConsole;
  if (AudioDeviceDescription::IsDefaultDevice(device_id_) ||
      AudioDeviceDescription::IsCommunicationsDevice(device_id_) ||
      AudioDeviceDescription::IsLoopbackDevice(device_id_)) {
    hr =
        enumerator->GetDefaultAudioEndpoint(data_flow, role, &endpoint_device_);
  } else {
    hr = enumerator->GetDevice(base::UTF8ToWide(device_id_).c_str(),
                               &endpoint_device_);
  }
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_ENDPOINT;
    return hr;
  }

  // Get the volume interface for the endpoint. Used in `Stop()` to query the
  // volume range of the selected input device or to get/set mute state in
  // `Start()` and `Stop()` if a loopback device with muted system audio is
  // requested.
  hr = endpoint_device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                  nullptr, &system_audio_volume_);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_ACTIVATION_FAILED;
    return hr;
  }

  // Verify that the audio endpoint device is active, i.e., the audio
  // adapter that connects to the endpoint device is present and enabled.
  DWORD state = DEVICE_STATE_DISABLED;
  hr = endpoint_device_->GetState(&state);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_STATE;
    return hr;
  }

  if (!(state & DEVICE_STATE_ACTIVE)) {
    DLOG(ERROR) << "Selected capture device is not active.";
    open_result_ = OPEN_RESULT_DEVICE_NOT_ACTIVE;
    hr = E_ACCESSDENIED;
  }

  return hr;
}

bool WASAPIAudioInputStream::RawProcessingSupported() {
  DCHECK(endpoint_device_.Get());
  // Check if System.Devices.AudioDevice.RawProcessingSupported can be found
  // and queried in the Windows Property System. It corresponds to raw
  // processing mode support for the specified audio device. If its value is
  // VARIANT_TRUE the device supports raw processing mode.
  bool raw_processing_supported = false;
  Microsoft::WRL::ComPtr<IPropertyStore> properties;
  base::win::ScopedPropVariant raw_processing;
  if (FAILED(endpoint_device_->OpenPropertyStore(STGM_READ, &properties)) ||
      FAILED(
          properties->GetValue(PKEY_Devices_AudioDevice_RawProcessingSupported,
                               raw_processing.Receive())) ||
      raw_processing.get().vt != VT_BOOL) {
    SendLogMessage(
        "%s => (WARNING: failed to access "
        "System.Devices.AudioDevice.RawProcessingSupported)",
        __func__);
  } else {
    raw_processing_supported = VariantBoolToBool(raw_processing.get().boolVal);
    SendLogMessage(
        "%s => (System.Devices.AudioDevice.RawProcessingSupported=%s)",
        __func__, raw_processing_supported ? "true" : "false");
  }
  return raw_processing_supported;
}

std::string WASAPIAudioInputStream::GetUWPDeviceId() {
  DCHECK(endpoint_device_.Get());

  // The Windows.Media.Devices.IMediaDeviceStatics interface provides access to
  // the implementation of Windows.Media.Devices.MediaDevice.
  ComPtr<IMediaDeviceStatics> media_device_statics;
  HRESULT hr =
      GetActivationFactory<IMediaDeviceStatics,
                           RuntimeClass_Windows_Media_Devices_MediaDevice>(
          &media_device_statics);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IMediaDeviceStatics factory failed: " << ErrorToString(hr);
    return std::string();
  }

  // The remaining part of this method builds up the unique device ID needed
  // by the Windows.Media.Effects.AudioEffectsManager UWP API to enumerate
  // active capture effects like AEC and NS. The ID contains three parts.
  // Example:
  //   1) \\?\SWD#MMDEVAPI#
  //   2) {0.0.1.00000000}.{7c24467c-94fc-4fa1-a2b2-a3f5d9cb8a5b}
  //   3) #{2eef81be-33fa-4800-9670-1cd474972c3f}
  // Where (1) is a constant string, (2) comes from the IMMDevice::GetId() API,
  // and (3) is a substring of of the selector string which can be retrieved by
  // the IMediaDeviceStatics::GetAudioCaptureSelector UWP API. Knowledge about
  // the structure of this device ID can be gained by using the
  // IMediaDeviceStatics::GetDefaultAudioCaptureId UWP API but this method also
  // adds support for non default devices.

  // (1) Start building the final device ID. Start with the constant prefix.
  std::string device_id(kUwpDeviceIdPrefix);

  // (2) Next, add the unique ID from IMMDevice::GetId() API.
  // Example: {0.0.1.00000000}.{7c24467c-94fc-4fa1-a2b2-a3f5d9cb8a5b}.
  ScopedCoMem<WCHAR> immdevice_id16;
  hr = endpoint_device_->GetId(&immdevice_id16);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IMMDevice::GetId failed: " << ErrorToString(hr);
    return std::string();
  }
  std::string immdevice_id8;
  base::WideToUTF8(immdevice_id16, wcslen(immdevice_id16), &immdevice_id8);
  device_id.append(immdevice_id8);

  // (3) Finally, add the last part from the selector string.
  // Example: '#{2eef81be-33fa-4800-9670-1cd474972c3f}'.
  HSTRING selector;
  // Returns the identifier string of a device for capturing audio. A substring
  // will be used when generating the final unique device ID.
  // Example: part of the selector string can look like
  // System.Devices.InterfaceClassGuid:="{2eef81be-33fa-4800-9670-1cd474972c3f}"
  // and we want the {2eef81be-33fa-4800-9670-1cd474972c3f} substring for our
  // purposes.
  hr = media_device_statics->GetAudioCaptureSelector(&selector);
  if (FAILED(hr)) {
    DLOG(ERROR) << "IMediaDeviceStatics::GetAudioCaptureSelector failed: "
                << ErrorToString(hr);
    return std::string();
  }
  device_id.append("#");
  std::string selector_string = ScopedHString(selector).GetAsUTF8();
  std::size_t start = selector_string.find("{");
  std::size_t stop = selector_string.find("}", start + 1);
  if (start != std::string::npos && stop != std::string::npos) {
    // Will extract '{2eef81be-33fa-4800-9670-1cd474972c3f}' in the example
    // above.
    device_id.append(selector_string.substr(start, stop - start + 1));
  } else {
    DLOG(ERROR) << "Failed to extract System.Devices.InterfaceClassGuid string";
    return std::string();
  }

  return device_id;
}

HRESULT WASAPIAudioInputStream::GetAudioCaptureEffects(
    const std::string& uwp_device_id) {
  DCHECK(!AudioDeviceDescription::IsLoopbackDevice(device_id_));
  DCHECK(raw_processing_supported_);
  DCHECK(!uwp_device_id.empty());
  SendLogMessage("%s()", __func__);

  // The Windows.Media.Effects.IAudioEffectsManagerStatics interface provides
  // access to the implementation of Windows.Media.Effects.AudioEffectsManager.
  ComPtr<IAudioEffectsManagerStatics> audio_effects_manager;
  HRESULT hr = GetActivationFactory<
      IAudioEffectsManagerStatics,
      RuntimeClass_Windows_Media_Effects_AudioEffectsManager>(
      &audio_effects_manager);
  if (FAILED(hr)) {
    SendLogMessage(
        "%s => (ERROR: IAudioEffectsManagerStatics factory failed: [%s])",
        __func__, ErrorToString(hr).c_str());
    return hr;
  }

  SendLogMessage("%s => (uwp_device_id=[%s])", __func__, uwp_device_id.c_str());
  ScopedHString device_id = ScopedHString::Create(uwp_device_id);

  // Check capture effects for two different audio processing modes:
  // - Default: Normal audio signal processing
  // - Raw: Minimal audio signal processing
  // Raw is included since it is not possible to disable all effects on all
  // devices. In most cases, the number of found capture effects will be zero
  // for the raw mode.
  ABI::Windows::Media::AudioProcessing audio_processing_mode[] = {
      ABI::Windows::Media::AudioProcessing::AudioProcessing_Default,
      ABI::Windows::Media::AudioProcessing::AudioProcessing_Raw};
  for (size_t i = 0; i < std::size(audio_processing_mode); ++i) {
    // Create an AudioCaptureEffectsManager manager which can be used to
    // discover the audio processing chain on a device for a specific media
    // category and audio processing mode. The media category is fixed and set
    // to Communications since that is what we aim at using when audio effects
    // later are disabled.
    ComPtr<IAudioCaptureEffectsManager> capture_effects_manager;
    hr = audio_effects_manager->CreateAudioCaptureEffectsManagerWithMode(
        device_id.get(),
        ABI::Windows::Media::Capture::MediaCategory::
            MediaCategory_Communications,
        audio_processing_mode[i], &capture_effects_manager);
    if (FAILED(hr)) {
      SendLogMessage(
          "%s => (ERROR: IAudioEffectsManagerStatics::"
          "CreateAudioCaptureEffectsManager=[%s])",
          __func__, ErrorToString(hr).c_str());
      return hr;
    }

    // Get a list of audio effects on the device. Based on tests on different
    // devices, only enabled effects will be included. Hence, if a user has
    // explicitly disabled an effect using the System Sound Settings, that
    // component will not show up here.
    ComPtr<IVectorView<ABI::Windows::Media::Effects::AudioEffect*>> effects;
    hr = capture_effects_manager->GetAudioCaptureEffects(&effects);
    if (FAILED(hr)) {
      SendLogMessage(
          "%s => (ERROR: IAudioCaptureEffectsManager::"
          "GetAudioCaptureEffects=[%s])",
          __func__, ErrorToString(hr).c_str());
      return hr;
    }

    unsigned int count = 0;
    if (effects) {
      // Returns number of supported effects.
      effects->get_Size(&count);
    }

    // Store all supported and active effect types in |default_effect_types_|
    // or |raw_effect_types_| depending on selected audio processing mode.
    // These will be utilized later for UMA histograms.
    for (unsigned int j = 0; j < count; ++j) {
      ComPtr<ABI::Windows::Media::Effects::IAudioEffect> effect;
      hr = effects->GetAt(j, &effect);
      if (SUCCEEDED(hr)) {
        ABI::Windows::Media::Effects::AudioEffectType type;
        hr = effect->get_AudioEffectType(&type);
        if (SUCCEEDED(hr)) {
          audio_processing_mode[i] ==
                  ABI::Windows::Media::AudioProcessing::AudioProcessing_Default
              ? default_effect_types_.push_back(type)
              : raw_effect_types_.push_back(type);
        }
      }
    }

    // For cases when no audio effects were found (common in raw mode), add a
    // dummy effect type called AudioEffectType_Other so that the vector
    // contains at least one value. This is done to ensure that an UMA histogram
    // is uploaded also for the empty case. Hence, AudioEffectType_Other is
    // used to indicate an unknown audio effect and "no audio effect found".
    if (count == 0) {
      const ABI::Windows::Media::Effects::AudioEffectType no_effect_found =
          ABI::Windows::Media::Effects::AudioEffectType::AudioEffectType_Other;
      audio_processing_mode[i] ==
              ABI::Windows::Media::AudioProcessing::AudioProcessing_Default
          ? default_effect_types_.push_back(no_effect_found)
          : raw_effect_types_.push_back(no_effect_found);
    }
  }

  return hr;
}

HRESULT WASAPIAudioInputStream::GetAudioEngineNumChannels(WORD* channels) {
  DCHECK(audio_client_.Get());
  SendLogMessage("%s()", __func__);
  WAVEFORMATEXTENSIBLE mix_format;
  // Retrieve the stream format that the audio engine uses for its internal
  // processing of shared-mode streams.
  HRESULT hr =
      CoreAudioUtil::GetSharedModeMixFormat(audio_client_.Get(), &mix_format);
  if (SUCCEEDED(hr)) {
    // Return the native number of supported audio channels.
    CoreAudioUtil::WaveFormatWrapper wformat(&mix_format);
    *channels = wformat->nChannels;
    SendLogMessage("%s => (native channels=[%d])", __func__, *channels);
  }
  return hr;
}

HRESULT
WASAPIAudioInputStream::SetCommunicationsCategoryAndMaybeRawCaptureMode(
    WORD channels) {
  DCHECK(audio_client_.Get());
  DCHECK(!AudioDeviceDescription::IsLoopbackDevice(device_id_));
  DCHECK(raw_processing_supported_);
  SendLogMessage("%s({channels=%d})", __func__, channels);

  Microsoft::WRL::ComPtr<IAudioClient2> audio_client2;
  HRESULT hr = audio_client_.As(&audio_client2);
  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: IAudioClient2 is not supported)", __func__);
    return hr;
  }
  // Use IAudioClient2::SetClientProperties() to set communications category
  // and to enable raw stream capture if it is supported.
  if (audio_client2.Get()) {
    AudioClientProperties audio_props = {0};
    audio_props.cbSize = sizeof(AudioClientProperties);
    audio_props.bIsOffload = false;
    // AudioCategory_Communications opts us in to communications policy and
    // communications processing. AUDCLNT_STREAMOPTIONS_RAW turns off the
    // processing, but not the policy.
    audio_props.eCategory = AudioCategory_Communications;
    // The audio stream is a 'raw' stream that bypasses all signal processing
    // except for endpoint specific, always-on processing in the Audio
    // Processing Object (APO), driver, and hardware.
    // See https://crbug.com/1257662 for details on why we avoid using raw
    // capture mode on devices with more than eight input channels.
    if (channels > 0 && channels <= media::kMaxConcurrentChannels) {
      audio_props.Options = AUDCLNT_STREAMOPTIONS_RAW;
    }
    hr = audio_client2->SetClientProperties(&audio_props);
    if (FAILED(hr)) {
      SendLogMessage("%s => (ERROR: IAudioClient2::SetClientProperties=[%s])",
                     __func__, ErrorToString(hr).c_str());
    }
  }
  return hr;
}

bool WASAPIAudioInputStream::DesiredFormatIsSupported(HRESULT* hr) {
  SendLogMessage("%s()", __func__);
  // An application that uses WASAPI to manage shared-mode streams can rely
  // on the audio engine to perform only limited format conversions. The audio
  // engine can convert between a standard PCM sample size used by the
  // application and the floating-point samples that the engine uses for its
  // internal processing. However, the format for an application stream
  // typically must have the same number of channels and the same sample
  // rate as the stream format used by the device.
  // Many audio devices support both PCM and non-PCM stream formats. However,
  // the audio engine can mix only PCM streams.
  base::win::ScopedCoMem<WAVEFORMATEX> closest_match;
  HRESULT hresult = audio_client_->IsFormatSupported(
      AUDCLNT_SHAREMODE_SHARED,
      reinterpret_cast<const WAVEFORMATEX*>(&input_format_), &closest_match);
  if (FAILED(hresult)) {
    SendLogMessage("%s => (ERROR: IAudioClient::IsFormatSupported=[%s])",
                   __func__, ErrorToString(hresult).c_str());
  }
  if (hresult == S_FALSE) {
    SendLogMessage(
        "%s => (WARNING: Format is not supported but a closest match exists)",
        __func__);
    // Change the format we're going to ask for to better match with what the OS
    // can provide.  If we succeed in initializing the audio client in this
    // format and are able to convert from this format, we will do that
    // conversion.
    WAVEFORMATEX* input_format = &input_format_.Format;
    input_format->nChannels = closest_match->nChannels;
    input_format->nSamplesPerSec = closest_match->nSamplesPerSec;

    // If the closest match is fixed point PCM (WAVE_FORMAT_PCM or
    // KSDATAFORMAT_SUBTYPE_PCM), we use the closest match's bits per sample.
    // Otherwise, we keep the bits sample as is since we still request fixed
    // point PCM. In that case the closest match is typically in float format
    // (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT).
    if (CoreAudioUtil::WaveFormatWrapper(closest_match.get()).IsPcm()) {
      input_format->wBitsPerSample = closest_match->wBitsPerSample;
    }

    input_format->nBlockAlign =
        (input_format->wBitsPerSample / 8) * input_format->nChannels;
    input_format->nAvgBytesPerSec =
        input_format->nSamplesPerSec * input_format->nBlockAlign;

    if (IsSupportedFormatForConversion(&input_format_)) {
      SendLogMessage(
          "%s => (WARNING: Captured audio will be converted: [%s] ==> [%s])",
          __func__, CoreAudioUtil::WaveFormatToString(&input_format_).c_str(),
          CoreAudioUtil::WaveFormatToString(&output_format_).c_str());
      SetupConverterAndStoreFormatInfo();

      // Indicate that we're good to go with a close match.
      hresult = S_OK;
    }
  }

  // At this point, |hresult| == S_OK if the desired format is supported. If
  // |hresult| == S_FALSE, the OS supports a closest match but we don't support
  // conversion to it. Thus, SUCCEEDED() or FAILED() can't be used to determine
  // if the desired format is supported.
  *hr = hresult;
  return (hresult == S_OK);
}

void WASAPIAudioInputStream::SetupConverterAndStoreFormatInfo() {
  // Ideally, we want a 1:1 ratio between the buffers we get and the buffers
  // we give to OnData so that each buffer we receive from the OS can be
  // directly converted to a buffer that matches with what was asked for.
  const double buffer_ratio =
      output_format_.nSamplesPerSec / static_cast<double>(packet_size_frames_);
  double new_frames_per_buffer =
      input_format_.Format.nSamplesPerSec / buffer_ratio;

  const auto input_layout =
      ChannelLayoutConfig::Guess(input_format_.Format.nChannels);
  DCHECK_NE(CHANNEL_LAYOUT_UNSUPPORTED, input_layout.channel_layout());
  const auto output_layout =
      ChannelLayoutConfig::Guess(output_format_.nChannels);
  DCHECK_NE(CHANNEL_LAYOUT_UNSUPPORTED, output_layout.channel_layout());

  const AudioParameters input(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                              input_layout, input_format_.Format.nSamplesPerSec,
                              static_cast<int>(new_frames_per_buffer));

  const AudioParameters output(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               output_layout, output_format_.nSamplesPerSec,
                               packet_size_frames_);

  converter_ = std::make_unique<AudioConverter>(input, output, false);
  converter_->AddInput(this);
  converter_->PrimeWithSilence();
  convert_bus_ = AudioBus::Create(output);

  // Update our packet size assumptions based on the new format.
  const auto new_bytes_per_buffer = static_cast<int>(new_frames_per_buffer) *
                                    input_format_.Format.nBlockAlign;
  packet_size_frames_ = new_bytes_per_buffer / input_format_.Format.nBlockAlign;
  packet_size_bytes_ = new_bytes_per_buffer;
  frame_size_bytes_ = input_format_.Format.nBlockAlign;

  imperfect_buffer_size_conversion_ =
      std::modf(new_frames_per_buffer, &new_frames_per_buffer) != 0.0;
  if (imperfect_buffer_size_conversion_) {
    SendLogMessage("%s => (WARNING: Audio capture conversion requires a FIFO)",
                   __func__);
  }
}

HRESULT WASAPIAudioInputStream::InitializeAudioEngine() {
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);
  SendLogMessage("%s()", __func__);

  DWORD flags;
  // Use event-driven mode only for regular input devices. For loopback the
  // EVENTCALLBACK flag is specified when initializing
  // |audio_render_client_for_loopback_|.
  if (AudioDeviceDescription::IsLoopbackDevice(device_id_)) {
    flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_NOPERSIST;
  } else {
    flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST;
  }

  // Initialize the audio stream between the client and the device.
  // We connect indirectly through the audio engine by using shared mode.
  // The buffer duration is set to 100 ms, which reduces the risk of glitches.
  // It would normally be set to 0 and the minimum buffer size to ensure that
  // glitches do not occur would be used (typically around 22 ms). There are
  // however cases when there are glitches anyway and it's avoided by setting a
  // larger buffer size. The larger size does not create higher latency for
  // properly implemented drivers.
  HRESULT hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED, flags,
      100 * 1000 * 10,  // Buffer duration, 100 ms expressed in 100-ns units.
      0,                // Device period, n/a for shared mode.
      reinterpret_cast<const WAVEFORMATEX*>(&input_format_),
      AudioDeviceDescription::IsCommunicationsDevice(device_id_)
          ? &kCommunicationsSessionId
          : nullptr);

  if (FAILED(hr)) {
    SendLogMessage("%s => (ERROR: IAudioClient::Initialize=[%s])", __func__,
                   ErrorToString(hr).c_str());
    open_result_ = OPEN_RESULT_AUDIO_CLIENT_INIT_FAILED;
    base::UmaHistogramSparse("Media.Audio.Capture.Win.InitError", hr);
    MaybeReportFormatRelatedInitError(hr);
    return hr;
  }

  // Retrieve the length of the endpoint buffer shared between the client
  // and the audio engine. The buffer length determines the maximum amount
  // of capture data that the audio engine can read from the endpoint buffer
  // during a single processing pass.
  hr = audio_client_->GetBufferSize(&endpoint_buffer_size_frames_);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_GET_BUFFER_SIZE_FAILED;
    return hr;
  }
  const int endpoint_buffer_size_ms =
      static_cast<double>(endpoint_buffer_size_frames_ * 1000) /
          input_format_.Format.nSamplesPerSec +
      0.5;
  SendLogMessage("%s => (endpoint_buffer_size_frames=%u (%d ms))", __func__,
                 endpoint_buffer_size_frames_, endpoint_buffer_size_ms);

#ifndef NDEBUG
  // The period between processing passes by the audio engine is fixed for a
  // particular audio endpoint device and represents the smallest processing
  // quantum for the audio engine. This period plus the stream latency between
  // the buffer and endpoint device represents the minimum possible latency
  // that an audio application can achieve.
  REFERENCE_TIME device_period_shared_mode = 0;
  REFERENCE_TIME device_period_exclusive_mode = 0;
  HRESULT hr_dbg = audio_client_->GetDevicePeriod(
      &device_period_shared_mode, &device_period_exclusive_mode);
  if (SUCCEEDED(hr_dbg)) {
    // The 5000 addition is to round end result to closest integer.
    const int device_period_ms = (device_period_shared_mode + 5000) / 10000;
    DVLOG(1) << "Device period: " << device_period_ms << " ms";
  }

  REFERENCE_TIME latency = 0;
  hr_dbg = audio_client_->GetStreamLatency(&latency);
  if (SUCCEEDED(hr_dbg)) {
    // The 5000 addition is to round end result to closest integer.
    const int latency_ms = (device_period_shared_mode + 5000) / 10000;
    DVLOG(1) << "Stream latency: " << latency_ms << " ms";
  }
#endif

  // Set the event handle that the audio engine will signal each time a buffer
  // becomes ready to be processed by the client.
  //
  // In loopback case the capture device doesn't receive any events, so we
  // need to create a separate playback client to get notifications. According
  // to MSDN:
  //
  //   A pull-mode capture client does not receive any events when a stream is
  //   initialized with event-driven buffering and is loopback-enabled. To
  //   work around this, initialize a render stream in event-driven mode. Each
  //   time the client receives an event for the render stream, it must signal
  //   the capture client to run the capture thread that reads the next set of
  //   samples from the capture endpoint buffer.
  //
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd316551(v=vs.85).aspx
  if (AudioDeviceDescription::IsLoopbackDevice(device_id_)) {
    SendLogMessage("%s => (WARNING: loopback mode is selected)", __func__);
    hr = endpoint_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    &audio_render_client_for_loopback_);
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_LOOPBACK_ACTIVATE_FAILED;
      return hr;
    }

    hr = audio_render_client_for_loopback_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, 0, 0,
        reinterpret_cast<const WAVEFORMATEX*>(&input_format_),
        AudioDeviceDescription::IsCommunicationsDevice(device_id_)
            ? &kCommunicationsSessionId
            : nullptr);
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_LOOPBACK_INIT_FAILED;
      return hr;
    }

    hr = audio_render_client_for_loopback_->SetEventHandle(
        audio_samples_ready_event_.Get());
  } else {
    hr = audio_client_->SetEventHandle(audio_samples_ready_event_.Get());
  }

  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_SET_EVENT_HANDLE;
    return hr;
  }

  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from the capture endpoint buffer.
  hr = audio_client_->GetService(IID_PPV_ARGS(&audio_capture_client_));
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_CAPTURE_CLIENT;
    return hr;
  }

  // Obtain a reference to the ISimpleAudioVolume interface which enables
  // us to control the master volume level of an audio session.
  hr = audio_client_->GetService(IID_PPV_ARGS(&simple_audio_volume_));
  if (FAILED(hr))
    open_result_ = OPEN_RESULT_NO_AUDIO_VOLUME;

  return hr;
}

void WASAPIAudioInputStream::ReportOpenResult(HRESULT hr) {
  DCHECK(!opened_);
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.Capture.Win.Open", open_result_,
                            OPEN_RESULT_MAX + 1);
  if (open_result_ != OPEN_RESULT_OK &&
      open_result_ != OPEN_RESULT_OK_WITH_RESAMPLING) {
    SendLogMessage(
        "%s", GetOpenLogString(open_result_, hr, input_format_, output_format_)
                  .c_str());
  }
}

void WASAPIAudioInputStream::MaybeReportFormatRelatedInitError(
    HRESULT hr) const {
  if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT && hr != E_INVALIDARG)
    return;

  const FormatRelatedInitError format_related_error =
      hr == AUDCLNT_E_UNSUPPORTED_FORMAT
          ? converter_.get()
                ? FormatRelatedInitError::kUnsupportedFormatWithFormatConversion
                : FormatRelatedInitError::kUnsupportedFormat
      // Otherwise |hr| == E_INVALIDARG.
      : converter_.get()
          ? FormatRelatedInitError::kInvalidArgumentWithFormatConversion
          : FormatRelatedInitError::kInvalidArgument;
  base::UmaHistogramEnumeration(
      "Media.Audio.Capture.Win.InitError.FormatRelated", format_related_error,
      FormatRelatedInitError::kCount);
}

double WASAPIAudioInputStream::ProvideInput(
    AudioBus* audio_bus,
    uint32_t frames_delayed,
    const AudioGlitchInfo& glitch_info) {
  fifo_->Consume()->CopyTo(audio_bus);
  return 1.0;
}

void WASAPIAudioInputStream::ReportAndResetGlitchStats() {
  glitch_accumulator_.GetAndReset();
  SystemGlitchReporter::Stats stats =
      glitch_reporter_.GetLongTermStatsAndReset();
  SendLogMessage(
      "%s => (num_glitches_detected=[%d], cumulative_audio_lost=[%llu ms], "
      "largest_glitch=[%llu ms])",
      __func__, stats.glitches_detected,
      stats.total_glitch_duration.InMilliseconds(),
      stats.largest_glitch_duration.InMilliseconds());

  int num_data_discontinuities =
      data_discontinuity_reporter_->GetLongTermDiscontinuityCountAndReset();
  SendLogMessage("%s => (discontinuity warnings=[%d])", __func__,
                 num_data_discontinuities);
  SendLogMessage("%s => (timstamp errors=[%" PRIu64 "])", __func__,
                 num_timestamp_errors_);
  if (num_timestamp_errors_ > 0) {
    SendLogMessage("%s => (time until first timestamp error=[%" PRId64 " ms])",
                   __func__,
                   time_until_first_timestamp_error_.InMilliseconds());
  }

  expected_next_device_position_ = 0;
  num_timestamp_errors_ = 0;
}

}  // namespace media
