// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_input_win.h"

#include <objbase.h>

#include <mmdeviceapi.h>

#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <combaseapi.h>
#include <ksmedia.h>
#include <propkey.h>
#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/scoped_variant.h"
#include "base/win/vector.h"
#include "base/win/windows_version.h"
#include "media/audio/application_loopback_device_helper.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_features.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/sample_format.h"
#include "media/base/timestamp_constants.h"

using base::win::ScopedCoMem;
using base::win::ScopedCOMInitializer;
using Microsoft::WRL::ComPtr;

namespace media {

namespace {

constexpr uint32_t KSAUDIO_SPEAKER_UNSUPPORTED = 0;

// Feature flag for enabling usage of the device/engine sample format.
BASE_FEATURE(kWasapiInputUseDeviceSampleFormat,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Max allowed absolute difference between a QPC-based timestamp and a default
// base::TimeTicks::Now() timestamp before switching to fake audio timestamps.
constexpr base::TimeDelta kMaxAbsTimeDiffBeforeSwithingToFakeTimestamps =
    base::Milliseconds(500);

// The System Process (PID 4) on Windows is a special kernel-level process that
// represents the Windows kernel itself. It is not a user-mode process and it
// does not host any executable code or services in the way normal processes do.
// Unlike most other processes, which get a dynamically assigned PID when they
// start, the System process consistently has PID 4 and by excluding all PIDs
// but this one we can capture all audio (not tied to any particular audio
// device) being played out.
constexpr uint32_t kWindowsSystemProcessId = 4;

// HRESULT_FROM_WIN32(WAIT_TIMEOUT) yields 0x80070102, which is a well-known COM
// error for timeouts.
constexpr HRESULT kActivationTimeoutHr = HRESULT_FROM_WIN32(WAIT_TIMEOUT);

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

WASAPIAudioInputStream::ActivateAudioInterfaceAsyncCallback&
GetActivateAudioInterfaceAsyncCallback() {
  static base::NoDestructor<
      WASAPIAudioInputStream::ActivateAudioInterfaceAsyncCallback>
      activate_audio_interface_async_callback{
          base::BindRepeating(&ActivateAudioInterfaceAsync)};
  return *activate_audio_interface_async_callback;
}

bool IsProcessLoopbackDevice(std::string_view device_id) {
  return device_id == AudioDeviceDescription::kLoopbackWithoutChromeId ||
         device_id == AudioDeviceDescription::kLoopbackAllDevicesId ||
         AudioDeviceDescription::IsApplicationLoopbackDevice(device_id);
}

uint32_t GetCurrentProcessId() {
  return static_cast<uint32_t>(base::Process::Current().Pid());
}

uint32_t GetTargetProcessId(std::string_view device_id) {
  if (AudioDeviceDescription::IsApplicationLoopbackDevice(device_id)) {
    return GetApplicationIdFromApplicationLoopbackDeviceId(device_id);
  }

  if (device_id == AudioDeviceDescription::kLoopbackWithoutChromeId) {
    return GetCurrentProcessId();
  }

  CHECK(AudioDeviceDescription::IsLoopbackDevice(device_id));
  return kWindowsSystemProcessId;
}

PROCESS_LOOPBACK_MODE GetProcessLoopbackMode(std::string_view device_id) {
  if (AudioDeviceDescription::IsApplicationLoopbackDevice(device_id)) {
    return PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
  }
  CHECK(AudioDeviceDescription::IsLoopbackDevice(device_id));
  return PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;
}

bool IsEndpointLoopbackCapture(std::string_view device_id,
                               bool is_process_loopback) {
  return AudioDeviceDescription::IsLoopbackDevice(device_id) &&
         !is_process_loopback;
}

SampleFormat GetSampleFormatFromWaveFormat(
    const WAVEFORMATEXTENSIBLE& wave_format) {
  switch (wave_format.Format.wBitsPerSample) {
    case 8:
      return kSampleFormatU8;
    case 16:
      return kSampleFormatS16;
    case 24:
      return kSampleFormatS24;
    case 32:
      if (IsEqualGUID(wave_format.SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        return kSampleFormatS32;
      } else if (IsEqualGUID(wave_format.SubFormat,
                             KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        return kSampleFormatF32;
      }
  }
  // We do not support other formats, return unknown.
  return kUnknownSampleFormat;
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

// Helper class to manage support of an echo canceller provided by either the
// device OEM or the OS.
class WASAPIAudioInputStream::EchoCancellationConfig {
 public:
  using LogCallback = base::RepeatingCallback<void(std::string)>;

  // Factory method which returns nullptr if system AEC is not supported.
  static std::unique_ptr<EchoCancellationConfig> Create(
      const AudioParameters& params,
      const std::string& device_id,
      LogCallback log_callback) {
    if (!(params.effects() & AudioParameters::ECHO_CANCELLER)) {
      return nullptr;
    }

    return base::WrapUnique(
        new EchoCancellationConfig(params, device_id, std::move(log_callback)));
  }

  // Builds up a string suitable for logging based on the effect `mask`.
  // Example: "#effects=2 (ECHO_CANCELLER | NOISE_SUPPRESSION)".
  static std::string GetSupportedEffectsString(int mask) {
    std::vector<std::string> effects;
    if (mask & AudioParameters::ECHO_CANCELLER) {
      effects.push_back("ECHO_CANCELLER");
    }
    if (mask & AudioParameters::NOISE_SUPPRESSION) {
      effects.push_back("NOISE_SUPPRESSION");
    }
    if (mask & AudioParameters::AUTOMATIC_GAIN_CONTROL) {
      effects.push_back("AUTOMATIC_GAIN_CONTROL");
    }

    std::string result;
    base::StringAppendF(&result, "%s => #effects=%zu (", __func__,
                        effects.size());
    for (size_t i = 0; i < effects.size(); ++i) {
      if (i > 0) {
        result += " | ";
      }
      result += effects[i];
    }
    result += ")";
    return result;
  }

  void LogMessage(std::string message) {
    if (log_callback_.is_null()) {
      return;
    }
    message.insert(0, "AEC::");
    log_callback_.Run(std::move(message));
  }

  PRINTF_FORMAT(2, 3) void LogMessage(const char* format, ...) {
    if (log_callback_.is_null()) {
      return;
    }
    va_list args;
    va_start(args, format);
    std::string msg(base::StrCat({"AEC::", base::StringPrintV(format, args)}));
    va_end(args);
    log_callback_.Run(std::move(msg));
  }

  // Enumerates supported voice processing audio effects (AEC, NS and AGC) and
  // logs the supported effect mask. Also performs an extra check that the
  // device really supports the AEC effect and logs an error if that is not the
  // case. Finally, it sets the preferred output device for the supported AEC.
  // Returns true if enumeration succeeded and AEC is among the supported
  // effects.
  bool SetAudioClientAndLogEffects(ComPtr<IAudioClient> audio_client) {
    CHECK(!AudioDeviceDescription::IsLoopbackDevice(device_id_));

    // We need an initialized audio client to be able to perform a correct check
    // of supported audio effects since we want to perform the check under the
    // exact same conditions as the stream will be opened under.
    CHECK(CoreAudioUtil::IsClientInitialized(audio_client.Get()));

    // Cache the initialized audio client.
    audio_client_ = audio_client;
    CHECK(audio_client_);

    // Find the supported voice processing effects and check if AEC is among
    // them. This call does not reinitialize the already initialized client.
    // Also triggers a "Media.Audio.Capture.Win.VoiceProcessingEffects"
    // histogram.
    auto [effects, echo_cancellation_is_available] =
        CoreAudioUtil::GetVoiceProcessingEffectsAndCheckForAEC(
            audio_client_.Get());
    if (effects != params_.effects()) {
      // Most probable cause for this state to happen is that some supported
      // effects have been disabled using constraints.
      LogMessage(
          "%s => (WARNING: supported effects do not match requested effects)",
          __func__);
    }

    // Set the preferred output device for the supported AEC.
    if (echo_cancellation_is_available) {
      UpdateEchoCancellationRenderEndpoint();
    } else {
      LogMessage("%s => (ERROR: system AEC is not supported)", __func__);
    }

    LogMessage(GetSupportedEffectsString(effects));
    return echo_cancellation_is_available;
  }

  // Set echo cancellation endpoint to `output_device_id_for_aec_` which is
  // kDefaultDeviceId unless it has been changed by SetOutputDeviceForAec().
  void UpdateEchoCancellationRenderEndpoint() {
    CHECK(audio_client_);

    // Use CoreAudioUtil::CreateDevice to create an IMMDevice since it also
    // checks that the selected device is active. The data-flow direction and
    // role are only utilized if the device ID is `kDefaultDeviceId` or
    // `kCommunicationsDeviceId`.
    ERole role = eConsole;
    if (AudioDeviceDescription::IsCommunicationsDevice(
            output_device_id_for_aec_)) {
      role = eCommunications;
    }
    ComPtr<IMMDevice> audio_device =
        CoreAudioUtil::CreateDevice(output_device_id_for_aec_, eRender, role);
    if (!audio_device.Get()) {
      LogMessage("%s => (ERROR: CoreAudioUtil::CreateDevice failed)", __func__);
      return;
    }

    AudioDeviceName device_name;
    CoreAudioUtil::GetDeviceName(audio_device.Get(), &device_name);
    LogMessage("%s => (AEC output device=[name: %s, id: %s])", __func__,
               device_name.device_name.c_str(), device_name.unique_id.c_str());

    // Get the IAcousticEchoCancellationControl interface using GetService.
    // Requires an initialized audio client and build 22621 or higher.
    ComPtr<IAcousticEchoCancellationControl> aec_control;
    HRESULT hr = audio_client_->GetService(IID_PPV_ARGS(&aec_control));
    if (FAILED(hr)) {
      LogMessage("%s => (ERROR: IAudioClient::GetService=[%s])", __func__,
                 ErrorToString(hr).c_str());
      return;
    }

    // Set the audio render endpoint that should be used as the reference
    // stream for acoustic echo cancellation (AEC). If it succeeds, the
    // capture endpoint supports control of the loopback reference endpoint
    // for AEC. Note that an endpoint may support AEC, but may not support
    // control of loopback reference endpoint for AEC. By default, the
    // system uses the default render device as the reference stream.
    std::wstring endpoint_id_wide = base::UTF8ToWide(device_name.unique_id);
    LPCWSTR endpoint_id = endpoint_id_wide.c_str();
    hr = aec_control->SetEchoCancellationRenderEndpoint(endpoint_id);
    if (FAILED(hr)) {
      LogMessage(
          "%s => (ERROR: "
          "IAcousticEchoCancellationControl::SetEchoCancellationRenderEndpoint="
          "[%s])",
          __func__, ErrorToString(hr).c_str());
    }
  }

  void SetOutputDeviceForAec(const std::string& output_device_id) {
    std::string new_output_device_id =
        output_device_id.empty() ? AudioDeviceDescription::kDefaultDeviceId
                                 : output_device_id;
    // Don't set an output device that's already in use.
    if (new_output_device_id == output_device_id_for_aec_) {
      return;
    }

    // Store the requested new ID to ensure that it can be utilized later if
    // a valid audio client does not exist yet.
    output_device_id_for_aec_ = new_output_device_id;

    // It is possible that an attempt to set the AEC render endpoint takes place
    // before a valid audio client exists. If so, simply store the device ID
    // and return.
    if (!audio_client_) {
      return;
    }

    // Set the new preferred AEC output.
    UpdateEchoCancellationRenderEndpoint();
  }

 private:
  explicit EchoCancellationConfig(const AudioParameters& params,
                                  const std::string& device_id,
                                  const LogCallback log_callback)
      : params_(params),
        device_id_(device_id),
        log_callback_(std::move(log_callback)) {}

  const AudioParameters params_;
  const std::string device_id_;

  // Stores log callback in outer WASAPIAudioInputStream class.
  // The resulting total prefix added to logs is "WAIS::AEC::".
  const LogCallback log_callback_;

  // Contains a copy of the main audio client in WASAPIAudioInputStream.
  ComPtr<IAudioClient> audio_client_;

  // Device ID corresponding to the audio render endpoint used as the reference
  // stream for acoustic echo cancellation (AEC). We use the default device as a
  // reference, unless something else was requested.
  std::string output_device_id_for_aec_ =
      AudioDeviceDescription::kDefaultDeviceId;
};

// Helper class to synchronously wait for the activation of an audio client
// during a call to ActivateAudioInterfaceAsync.
class WASAPIAudioInputStream::AudioClientActivationHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IActivateAudioInterfaceCompletionHandler> {
 public:
  friend class FakeWinWASAPIEnvironment;

  AudioClientActivationHandler() = default;
  ~AudioClientActivationHandler() override = default;

  // When called, returns only after the activation is completed.
  HRESULT WaitAndGetAudioClient(ComPtr<IAudioClient>* audio_client,
                                base::TimeDelta async_activation_timeout_ms) {
    // Wait for a maximum of 10 seconds for the activation to complete.
    if (!wait_event_.TimedWait(async_activation_timeout_ms)) {
      return kActivationTimeoutHr;
    }

    // If the activation was successful, move the audio client to the output
    // parameter.
    if (SUCCEEDED(activation_result_)) {
      *audio_client = std::move(audio_client_);
    }
    return activation_result_;
  }

 private:
  // IActivateAudioInterfaceAudioClientActivationHandler::ActivateCompleted
  // implementation.
  // Called by the OS when the activation is completed.
  IFACEMETHODIMP ActivateCompleted(
      IActivateAudioInterfaceAsyncOperation* activate_operation) override {
    HRESULT hr_activate = S_OK;
    ComPtr<IAudioClient> audio_client = nullptr;
    activation_result_ =
        activate_operation->GetActivateResult(&hr_activate, &audio_client);
    if (FAILED(activation_result_)) {
      return activation_result_;
    }

    activation_result_ = hr_activate;
    if (SUCCEEDED(activation_result_)) {
      audio_client_ = std::move(audio_client);
    }
    wait_event_.Signal();

    // If the activation was successful, the audio client is now available.
    return activation_result_;
  }

  ComPtr<IAudioClient> audio_client_ = nullptr;
  HRESULT activation_result_ = E_FAIL;
  base::WaitableEvent wait_event_{
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

// Creates an audio input stream given preferred audio parameters in `params`
// and an input device given by `device_id`.
// Support for system effects exists behind a command-line flag called
// `media::EnforceSystemEchoCancellation` and it will have an effect on the
// content in `params` and especially in the `effects` part of `params`.
// Audio parameters are enumerated and set in
// AudioManagerWin::GetInputStreamParameters() for the `device_id` and they
// represent the "most suitable" parameters for the given device in terms of
// number of audio channels, sample rate etc. But they also contain a special
// part given by params.effects() which is a bitwise OR combination of effect
// flags (e.g. ECHO_CANCELLER | NOISE_SUPPRESSION | AUTOMATIC_GAIN_CONTROL).
// These are the results of a previous check of supported system effects for the
// specified device in CoreAudioUtil::GetVoiceProcessingEffectsAndCheckForAEC().
// To be able to enumerate them, an IAudioClient object must be initialized and
// then used to create an IAudioEffectsManager which then supports a API called
// GetAudioEffects(). System effects are only supported if the audio client is
// working in a "raw" mode and if it is set to a communications mode and these
// details are done explicitly in GetInputStreamParameters() and then the
// audio client is closed and destroyed. It means that params.effects() will
// contain information about what effects that *should" be supported.
// The parameters are provided to a helper class called EchoCancellationConfig
// which is stored in `aec_config_`. It will only be a valid object (not null)
// if `params` contains ECHO_CANCELLER. The other effects are not analyzed.
// The existence of the AEC helper class object will changed the behavior of
// WASAPIAudioInputStream in the following way:
// - In Open(): the audio client properties will be changed from
//   AUDCLNT_STREAMOPTIONS_RAW to AUDCLNT_STREAMOPTIONS_NONE since RAW would
//   bypass all supported effects (and we we are now asked to support AEC).
// - In Open(): an additional enumeration of the supported audio effects is
//   done by EchoCancellationConfig to ensure that we log what this stream
//   actually uses and that the AEC *really* is enabled.
// This last step uses CoreAudioUtil::GetVoiceProcessingEffectsAndCheckForAEC(),
// as was done during the previous enumeration, but this time an already
// initialized audio client is utilized. Hence, there is no extra "dummy"
// initialization taking place this time since we already know that this stream
// will use a non-raw audio client in communications mode.
// Additional details:
// - This class only looks for AEC in the requested effect parameter, hence
//   NOISE_SUPPRESSION | AUTOMATIC_GAIN_CONTROL would result in an input stream
//   with *no* effects.
// - Even if AEC is the key effect here, there is no way to enable only the AEC
//   effect since it always comes as a "package" with AEC and NS and AGC.
// - When an AEC is requested, this class will update the
//   Media.Audio.Capture.Win.VoiceProcessingEffects histogram when the stream
//   is opened. The same histogram is also updated during the enumeration.
WASAPIAudioInputStream::WASAPIAudioInputStream(
    AudioManagerWin* manager,
    const AudioParameters& params,
    const std::string& device_id,
    AudioManager::LogCallback log_callback)
    : manager_(manager),
      params_(params),
      peak_detector_(base::BindRepeating(&AudioManager::TraceAmplitudePeak,
                                         base::Unretained(manager_),
                                         /*trace_start=*/true)),
      data_discontinuity_reporter_(
          std::make_unique<DataDiscontinuityReporter>()),
      device_id_(device_id),
      log_callback_(std::move(log_callback)),
      aec_config_(EchoCancellationConfig::Create(
          params,
          device_id,
          base::BindRepeating(
              static_cast<void (WASAPIAudioInputStream::*)(std::string)>(
                  &WASAPIAudioInputStream::SendLogMessage),
              base::Unretained(this)))),
      is_loopback_capture_(AudioDeviceDescription::IsLoopbackDevice(device_id)),
      is_process_loopback_capture_(IsProcessLoopbackDevice(device_id)),
      glitch_reporter_(is_loopback_capture_
                           ? SystemGlitchReporter::StreamType::kLoopback
                           : SystemGlitchReporter::StreamType::kCapture) {
  DCHECK(manager_);
  DCHECK(!device_id_.empty());
  DCHECK(!log_callback_.is_null());
  DCHECK_LE(params.channels(), 2);
  DCHECK(params.channel_layout() == CHANNEL_LAYOUT_MONO ||
         params.channel_layout() == CHANNEL_LAYOUT_STEREO ||
         params.channel_layout() == CHANNEL_LAYOUT_DISCRETE);
  SendLogMessage("%s({device_id=%s}, {params=[%s]})", __func__,
                 device_id.c_str(), params.AsHumanReadableString().c_str());
  if (AudioDeviceDescription::IsLoopbackDevice(device_id_)) {
    SendLogMessage("%s => (audio loopback device is of type: %s)", __func__,
                   is_process_loopback_capture_ ? "PROCESS" : "ENDPOINT");
  }
  SendLogMessage("%s => (AEC is requested=[%s])", __func__,
                 aec_config_ ? "true" : "false");

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  if (!avrt_init)
    SendLogMessage("%s => (WARNING: failed to load Avrt.dll)", __func__);

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_ready_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_ready_event_.is_valid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_capture_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_capture_event_.is_valid());

  use_device_sample_format_ =
      base::FeatureList::IsEnabled(kWasapiInputUseDeviceSampleFormat);
}

WASAPIAudioInputStream::~WASAPIAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool WASAPIAudioInputStream::UpdateFormats() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sample_format_ = kSampleFormatS16;
  input_format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  output_format_.wFormatTag = WAVE_FORMAT_PCM;
  if (use_device_sample_format_) {
    // Get the format the audio engine uses.
    WAVEFORMATEXTENSIBLE mix_format;
    HRESULT hr =
        CoreAudioUtil::GetSharedModeMixFormat(audio_client_.Get(), &mix_format);
    if (FAILED(hr)) {
      ReportOpenResult(hr);
      return false;
    }
    // Note that Windows Audio Engine could potentially be S32 or F32.
    auto mix_sample_format = GetSampleFormatFromWaveFormat(mix_format);
    base::UmaHistogramEnumeration("Media.Audio.Capture.Win.AudioEngineFormat",
                                  mix_sample_format);
    if (mix_sample_format != kUnknownSampleFormat) {
      sample_format_ = mix_sample_format;
      // We are not sure if the Windows Audio Engine will ever choose 24bit over
      // 32bit. Check if this is the case, and if so we choose S32 instead.
      CHECK_NE(sample_format_, kSampleFormatS24, base::NotFatalUntil::M148);
      if (sample_format_ == kSampleFormatS24) {
        sample_format_ = kSampleFormatS32;
      }

      input_format_.SubFormat = mix_format.SubFormat;
      // Set up the fixed output format based on the discovered format.
      if (IsEqualGUID(mix_format.SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        CHECK_NE(sample_format_, kSampleFormatF32);
        output_format_.wFormatTag = WAVE_FORMAT_PCM;
      } else if (IsEqualGUID(mix_format.SubFormat,
                             KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        CHECK_EQ(sample_format_, kSampleFormatF32);
        output_format_.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
      } else {
        // We don't support other wFormatTags, consider this as failed.
        return false;
      }
    } else {
      const uint32_t format_tag =
          EXTRACT_WAVEFORMATEX_ID(&mix_format.SubFormat);
      base::UmaHistogramSparse(
          "Media.Audio.Capture.Win.AudioEngineFormat.Unknown", format_tag);

      use_device_sample_format_ = false;
    }
  }

  // The clients asks for an input stream specified by `params`. Start by
  // setting up an input device format according to the same specification.
  // If all goes well during the upcoming initialization, this format will not
  // change. However, under some circumstances, minor changes can be required
  // to fit the current input audio device. If so, a FIFO and/or and audio
  // converter might be needed to ensure that the output format of this stream
  // matches what the client asks for.
  WAVEFORMATEX* format = &input_format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = params_.channels();
  format->nSamplesPerSec = params_.sample_rate();
  format->wBitsPerSample = SampleFormatToBitsPerChannel(sample_format_);
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;

  // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE which can be
  // required in combination with e.g. multi-channel microphone arrays.
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  input_format_.Samples.wValidBitsPerSample = format->wBitsPerSample;
  input_format_.dwChannelMask =
      ChannelLayoutToChannelConfig(params_.channel_layout());
  SendLogMessage("%s => (audio engine format=[%s])", __func__,
                 CoreAudioUtil::WaveFormatToString(&input_format_).c_str());

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
  packet_size_bytes_ = params_.GetBytesPerBuffer(sample_format_);
  packet_size_frames_ = packet_size_bytes_ / format->nBlockAlign;
  SendLogMessage(
      "%s => (packet size=[%zu bytes/%zu audio frames/%.3f milliseconds])",
      __func__, packet_size_bytes_, packet_size_frames_,
      params_.GetBufferDuration().InMillisecondsF());
  return true;
}

AudioInputStream::OpenOutcome WASAPIAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SendLogMessage("%s([opened=%s])", __func__, opened_ ? "true" : "false");
  if (opened_) {
    return OpenOutcome::kAlreadyOpen;
  }

  HRESULT hr = S_OK;
  // Process loopback captures do not get audio from an endpoint device, but
  // rather from an audio interface.
  if (!is_process_loopback_capture_) {
    // Obtain a reference to the IMMDevice interface of the capturing device
    // with the specified unique identifier or role which was set at
    // construction.
    hr = SetCaptureDevice();
    if (FAILED(hr)) {
      ReportOpenResult(hr);
      return OpenOutcome::kFailed;
    }
  }

  // Activate the AudioClient interface. This is done differently depending on
  // whether the device is a process loopback device or not. For process
  // loopback devices, a special activation method must be used to activate the
  // audio client asynchronously.
  hr = ActivateAudioClientInterface();
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_ACTIVATION_FAILED;
    ReportOpenResult(hr);
    return OpenOutcome::kFailed;
  }

  if (!UpdateFormats()) {
    return OpenOutcome::kFailed;
  }

  // Process loopback captures do not get audio from an endpoint device.
  if (!is_process_loopback_capture_) {
    // Check if raw audio processing is supported for the selected capture
    // device.
    raw_processing_supported_ = RawProcessingSupported();
  }

  // Raw audio capture suppresses processing that down mixes e.g. a microphone
  // array into a supported format and instead exposes the device's native
  // format. Chrome only supports a maximum number of input channels given by
  // media::kMaxConcurrentChannels. Therefore, one additional test is needed
  // before stating that raw audio processing can be supported.
  // Failure will not prevent opening but the method must succeed to be able
  // to select raw input capture mode.
  WORD audio_engine_channels = 0;
  if (!AudioDeviceDescription::IsLoopbackDevice(device_id_)) {
    hr = GetAudioEngineNumChannels(&audio_engine_channels);
  }

  // Attempt to enable communications category and raw capture mode on the
  // audio stream. Ignoring return value since the method logs its own error
  // messages and it should be OK to continue opening the stream even after a
  // failure.
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

  // Enumerate all supported audio effects and set the preferred output device
  // for the AEC if AEC is requested and supported. These operations require an
  // initialized audio client.
  if (aec_config_) {
    if (!aec_config_->SetAudioClientAndLogEffects(audio_client_)) {
      aec_config_.reset();
    }
  }

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
    if (FAILED(hr)) {
      SendLogMessage(
          "%s => (ERROR: IAudioClient::Start=[%s] (endpoint loopback))",
          __func__, ErrorToString(hr).c_str());
    }
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
  if (stop_capture_event_.is_valid()) {
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
  if (!opened_ || !simple_audio_volume_) {
    return;
  }

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
  if (!simple_audio_volume_) {
    return 0.0;
  }

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
  if (!simple_audio_volume_) {
    return false;
  }

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
  SendLogMessage("%s({output_device_id=%s})", __func__,
                 output_device_id.c_str());
  if (aec_config_) {
    aec_config_->SetOutputDeviceForAec(output_device_id);
  }
}

void WASAPIAudioInputStream::SendLogMessage(std::string message) {
  if (log_callback_.is_null()) {
    return;
  }
  message.insert(0, "WAIS::");
  log_callback_.Run(std::move(message));
}

void WASAPIAudioInputStream::SendLogMessage(const char* format, ...) {
  if (log_callback_.is_null())
    return;
  va_list args;
  va_start(args, format);
  std::string msg(base::StrCat({"WAIS::", base::StringPrintV(format, args)}));
  va_end(args);
  log_callback_.Run(std::move(msg));
}

// static
void WASAPIAudioInputStream::
    OverrideActivateAudioInterfaceAsyncCallbackForTesting(
        ActivateAudioInterfaceAsyncCallback callback) {
  GetActivateAudioInterfaceAsyncCallback() = callback;
}

void WASAPIAudioInputStream::SimulateErrorForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(capture_thread_);
  simulate_error_for_testing_ = true;
}

HRESULT WASAPIAudioInputStream::CreateFifoIfNeeded() {
  if (fifo_) {
    return S_OK;
  }

  // Retrieve the length of the endpoint buffer shared between the client
  // and the audio engine. The buffer length determines the maximum amount
  // of capture data that the audio engine can read from the endpoint buffer
  // during a single processing pass.
  uint32_t endpoint_buffer_size_frames = 0;
  HRESULT hr = audio_client_->GetBufferSize(&endpoint_buffer_size_frames);
  if (FAILED(hr)) {
    return hr;
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
      std::max(2 * endpoint_buffer_size_frames * frame_size_bytes_,
               2 * packet_size_frames_ * frame_size_bytes_);
  int buffers_required = capture_buffer_size / packet_size_bytes_;
  if (converter_ && imperfect_buffer_size_conversion_)
    ++buffers_required;

  DCHECK(!fifo_);
  fifo_ = std::make_unique<AudioBlockFifo>(
      input_format_.Format.nChannels, packet_size_frames_, buffers_required);
  DVLOG(1) << "AudioBlockFifo buffer count: " << buffers_required;
  return S_OK;
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

    // Test-only hook to simulate a failure in the capture loop.
    if (simulate_error_for_testing_) {
      wait_result = WAIT_FAILED;
      simulate_error_for_testing_ = false;
    }

    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_capture_event_| has been set.
        recording = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |audio_samples_ready_event_| has been set.
        CreateFifoIfNeeded();
        if (!fifo_) {
          // An error happened while creating the FIFO.
          error = true;
          LOG(ERROR) << "WAIS::" << __func__
                     << " => (ERROR: failed to create FIFO)";
          break;
        }
        PullCaptureDataAndPushToSink();
        break;
      case WAIT_FAILED:
      default:
        error = true;
        break;
    }
  }

  if (recording && error) {
    auto saved_last_error = GetLastError();
    LOG(ERROR) << "WAIS::" << __func__
               << " => (ERROR: capturing failed with error code: "
               << saved_last_error << ")";
    // Stop audio rendering since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly. This approach is inline with the
    // design in WASAPIAudioOutputStream.
    audio_client_->Stop();

    // There was an error while recording audio.
    sink_->OnError();
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
              glitch_duration, is_loopback_capture_
                                   ? AudioGlitchInfo::Direction::kLoopback
                                   : AudioGlitchInfo::Direction::kCapture));
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
      if (is_process_loopback_capture_) {
        DCHECK_EQ(device_position, 0u);
      } else {
        DCHECK_GT(device_position, 0u);
      }
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

      // SAFETY:
      // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer
      // `data_ptr` is the starting address of the next data packet read.
      //
      // `num_frames_to_read` is the frame count (number of audio frames
      // available in the packet).
      //
      // The document also mentions: The size of a frame in an audio stream is
      // specified by the `nBlockAlign` member of the WAVEFORMATEX (or
      // WAVEFORMATEXTENSIBLE) structure that specifies the stream format. The
      // size, in bytes, of an audio frame equals the number of channels in the
      // stream multiplied by the sample size per channel. For example, for a
      // stereo (2-channel) stream with 16-bit samples, the frame size is four
      // bytes.
      //
      // So actually in bytes. Our size is `num_frames_to_read` *
      // `input_format_.Format.nBlockAlign`.
      CHECK_EQ(input_format_.Format.nBlockAlign,
               bytes_per_sample * input_format_.Format.nChannels);
      UNSAFE_BUFFERS(base::span<const uint8_t> audio_frames(
          reinterpret_cast<const uint8_t*>(data_ptr),
          base::CheckMul<size_t>(num_frames_to_read,
                                 input_format_.Format.nBlockAlign)
              .ValueOrDie()));
      peak_detector_.FindPeak(audio_frames, sample_format_);
      fifo_->Push(audio_frames, num_frames_to_read, sample_format_);
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
  NOTREACHED() << "Error code: " << err;
}

HRESULT WASAPIAudioInputStream::SetCaptureDevice() {
  DCHECK(!is_process_loopback_capture_);
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

HRESULT WASAPIAudioInputStream::ActivateAudioClientInterface() {
  if (!is_process_loopback_capture_) {
    // Obtain an IAudioClient interface for the endpoint device which enables us
    // to create and initialize an audio stream between an audio application and
    // the audio engine.
    return endpoint_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                      nullptr, &audio_client_);
  }

  CHECK(is_process_loopback_capture_);
  // Detailed information about AUDIOCLIENT_ACTIVATION_PARAMS can be found at:
  // https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ns-audioclientactivationparams-audioclient_activation_params
  AUDIOCLIENT_ACTIVATION_PARAMS params = {
      //  Specify the process capture.
      .ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK,
      // The following combinations of loopback device ID, target process ID and
      // process loopback mode are supported:
      //
      // | --------------|------------------|-----------------------------|
      // |   device_id   | TargetProcessId  |     ProcessLoopbackMode     |
      // | --------------|------------------|-----------------------------|
      // | Application   | application PID  | INCLUDE_TARGET_PROCESS_TREE |
      // | WithoutChrome | Chrome audio PID | EXCLUDE_TARGET_PROCESS_TREE |
      // | AllDevices    |       4          | EXCLUDE_TARGET_PROCESS_TREE |
      // | --------------|------------------|-----------------------------|
      .ProcessLoopbackParams =
          {
              // Set the target process ID based on the selected loopback audio
              // capture device.
              .TargetProcessId = GetTargetProcessId(device_id_),
              // The captured audio either includes or excludes audio rendered
              // by `TargetProcessId` and its child processes.
              .ProcessLoopbackMode = GetProcessLoopbackMode(device_id_),
          },
  };
  PROPVARIANT propvariant = {
      .vt = VT_BLOB,
      .blob =
          {
              .cbSize = sizeof(params),
              .pBlobData = reinterpret_cast<BYTE*>(&params),
          },
  };

  TRACE_EVENT("audio", "AudioClientActivation");
  base::ElapsedTimer timer;
  ComPtr<AudioClientActivationHandler> completion_handler =
      Microsoft::WRL::Make<AudioClientActivationHandler>();
  ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
  HRESULT hr = GetActivateAudioInterfaceAsyncCallback().Run(
      VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
      &propvariant, completion_handler.Get(), &async_op);
  if (FAILED(hr)) {
    TRACE_EVENT_INSTANT0("audio", "ActivateAudioInterfaceAsync failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return hr;
  }

  hr = completion_handler->WaitAndGetAudioClient(&audio_client_,
                                                 async_activation_timeout_ms_);
  const bool timed_out = (hr == kActivationTimeoutHr);
  base::UmaHistogramBoolean("Media.Audio.Capture.Win.GetAudioClientTimedOut",
                            timed_out);
  if (!timed_out) {
    base::UmaHistogramTimes("Media.Audio.Capture.Win.TimeToGetAudioClient",
                            timer.Elapsed());
  } else {
    TRACE_EVENT_INSTANT0("audio", "GetAudioClient timed out",
                         TRACE_EVENT_SCOPE_THREAD);
  }

  return hr;
}

bool WASAPIAudioInputStream::RawProcessingSupported() {
  DCHECK(!is_process_loopback_capture_);
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
    // Use AUDCLNT_STREAMOPTIONS_NONE instead of AUDCLNT_STREAMOPTIONS_RAW if
    // system AEC has been enabled to ensure that "Voice Clarity" can kick in.
    // From Win11 24H2, apps which use Communications Signal Processing Mode
    // do not need any additional modifications and Voice Clarity will work for
    // them automatically when the OEM device does not offer Communications Mode
    // processing.
    if (aec_config_) {
      audio_props.Options = AUDCLNT_STREAMOPTIONS_NONE;
      SendLogMessage("%s => (WARNING: attempting to enable system AEC)",
                     __func__);
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

  // Process loopback mode is a virtual device. Therefore, neither
  // IAudioClient::GetMixFormat nor IAudioClient::IsFormatSupported are
  // supported. We are free to pick whichever format we want and can pass it
  // into the call to IAudioClient::Initialize.
  if (is_process_loopback_capture_) {
    return true;
  }

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
    if (CoreAudioUtil::WaveFormatWrapper(closest_match.get()).IsPcm() &&
        input_format->wBitsPerSample != closest_match->wBitsPerSample) {
      // Enabling kWasapiInputUseDeviceSampleFormat allows us to query the Audio
      // Engine for its MixFormat. The MixFormat should in theory always be
      // supported and have the same bit depth, so we should not hit this
      // pathway.
      CHECK(!use_device_sample_format_, base::NotFatalUntil::M148);
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

  // Use event-driven mode only for regular input devices or process loopback.
  // Loopback devices capturing from an endpoint device does not support event-
  // driven mode since it requires active output audio to trigger the event.
  // For endpoint loopback devices, EVENTCALLBACK flag is specified when
  // initializing the extra |audio_render_client_for_loopback_|.
  DWORD flags =
      IsEndpointLoopbackCapture(device_id_, is_process_loopback_capture_)
          ? 0
          : AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  if (!is_process_loopback_capture_) {
    // Process loopback capture does not support the
    // AUDCLNT_STREAMFLAGS_NOPERSIST flag.
    flags |= AUDCLNT_STREAMFLAGS_NOPERSIST;
  }
  if (AudioDeviceDescription::IsLoopbackDevice(device_id_)) {
    // Create a loopback stream that captures what the system is playing
    // instead of the microphone input.
    flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
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

  // TODO(https://crbug.com/411452039): Waiting for the first audio sample ready
  // event to be signaled is only needed for process loopback devices. We need
  // to do it because, due to a Windows bug, the value returned by
  // IAudioClient::GetBufferSize() can not be trusted until we get the first
  // sample.
  if (!is_process_loopback_capture_) {
    hr = CreateFifoIfNeeded();
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_GET_BUFFER_SIZE_FAILED;
      return hr;
    }
  }

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
    const int latency_ms = (latency + 5000) / 10000;
    DVLOG(1) << "Stream latency: " << latency_ms << " ms";
  }
#endif

  // Set the event handle that the audio engine will signal each time a buffer
  // becomes ready to be processed by the client.
  //
  // In endpoint loopback mode the capture device is not running in an event-
  // driven mode so we need to create a separate playback client to get
  // notifications.
  if (IsEndpointLoopbackCapture(device_id_, is_process_loopback_capture_)) {
    SendLogMessage("%s => (WARNING: endpoint loopback mode is selected)",
                   __func__);
    hr = endpoint_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    &audio_render_client_for_loopback_);
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_LOOPBACK_ACTIVATE_FAILED;
      return hr;
    }

    // To ensure that we can deliver a loopback stream capturing an audio
    // endpoint also when no output audio is playing, we initialize a render
    // stream in event-driven mode. Each time the client receives an event for
    // the render stream, it must signal the capture client to run the capture
    // thread that reads the next set of samples from the capture endpoint
    // buffer. Note that |input_format_| corresponds to the preferred parameters
    // of the default output device in loopback mode.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd316551(v=vs.85).aspx
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

  // WASAPI does not allow the AudioClient to control the process loopback
  // device volume. The AudioEndpointVolume interface is not available for
  // process loopback devices.
  if (!is_process_loopback_capture_) {
    // Obtain a reference to the ISimpleAudioVolume interface which enables
    // us to control the master volume level of an audio session.
    hr = audio_client_->GetService(IID_PPV_ARGS(&simple_audio_volume_));
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_NO_AUDIO_VOLUME;
    }
  }

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
  CHECK_DEREF(fifo_.get()).Consume()->CopyTo(audio_bus);
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
