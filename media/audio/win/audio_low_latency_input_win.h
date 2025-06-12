// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of AudioInputStream for Windows using Windows Core Audio
// WASAPI for low latency capturing.
//
// Overview of operation:
//
// - An object of WASAPIAudioInputStream is created by the AudioManager
//   factory.
// - Next some thread will call Open(), at that point the underlying
//   Core Audio APIs are utilized to create two WASAPI interfaces called
//   IAudioClient and IAudioCaptureClient.
// - Then some thread will call Start(sink).
//   A thread called "wasapi_capture_thread" is started and this thread listens
//   on an event signal which is set periodically by the audio engine for
//   each recorded data packet. As a result, data samples will be provided
//   to the registered sink.
// - At some point, a thread will call Stop(), which stops and joins the
//   capture thread and at the same time stops audio streaming.
// - The same thread that called stop will call Close() where we cleanup
//   and notify the audio manager, which likely will destroy this object.
//
// Implementation notes:
//
// - The minimum supported client is Windows Vista.
// - This implementation is single-threaded, hence:
//    o Construction and destruction must take place from the same thread.
//    o It is recommended to call all APIs from the same thread as well.
// - It is recommended to first acquire the native sample rate of the default
//   input device and then use the same rate when creating this object. Use
//   WASAPIAudioInputStream::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
//
// Core Audio API details:
//
// - Utilized MMDevice interfaces:
//     o IMMDeviceEnumerator
//     o IMMDevice
// - Utilized WASAPI interfaces:
//     o IAudioClient
//     o IAudioCaptureClient
// - The stream is initialized in shared mode and the processing of the
//   audio buffer is event driven.
// - The Multimedia Class Scheduler service (MMCSS) is utilized to boost
//   the priority of the capture thread.
// - Audio applications that use the MMDevice API and WASAPI typically use
//   the ISimpleAudioVolume interface to manage stream volume levels on a
//   per-session basis. It is also possible to use of the IAudioEndpointVolume
//   interface to control the master volume level of an audio endpoint device.
//   This implementation is using the ISimpleAudioVolume interface.
//   MSDN states that "In rare cases, a specialized audio application might
//   require the use of the IAudioEndpointVolume".
//
#ifndef MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_

#include <MMDeviceAPI.h>

#include <Audioclient.h>
#include <endpointvolume.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/system_glitch_reporter.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioBlockFifo;
class AudioBus;

// AudioInputStream implementation using Windows Core Audio APIs.
class MEDIA_EXPORT WASAPIAudioInputStream
    : public AgcAudioStream<AudioInputStream>,
      public base::DelegateSimpleThread::Delegate,
      public AudioConverter::InputCallback {
 public:
  // Used to track down where we fail during initialization which at the
  // moment seems to be happening frequently and we're not sure why.
  // The reason might be expected (e.g. trying to open "default" on a machine
  // that has no audio devices).
  // Note: This enum is used to record a histogram value and should not be
  // re-ordered.
  enum StreamOpenResult {
    OPEN_RESULT_OK = 0,
    OPEN_RESULT_CREATE_INSTANCE = 1,
    OPEN_RESULT_NO_ENDPOINT = 2,
    OPEN_RESULT_NO_STATE = 3,
    OPEN_RESULT_DEVICE_NOT_ACTIVE = 4,
    OPEN_RESULT_ACTIVATION_FAILED = 5,
    OPEN_RESULT_FORMAT_NOT_SUPPORTED = 6,
    OPEN_RESULT_AUDIO_CLIENT_INIT_FAILED = 7,
    OPEN_RESULT_GET_BUFFER_SIZE_FAILED = 8,  // Obsolete.
    OPEN_RESULT_LOOPBACK_ACTIVATE_FAILED = 9,
    OPEN_RESULT_LOOPBACK_INIT_FAILED = 10,
    OPEN_RESULT_SET_EVENT_HANDLE = 11,
    OPEN_RESULT_NO_CAPTURE_CLIENT = 12,
    OPEN_RESULT_NO_AUDIO_VOLUME = 13,
    OPEN_RESULT_OK_WITH_RESAMPLING = 14,
    OPEN_RESULT_MAX = OPEN_RESULT_OK_WITH_RESAMPLING
  };

  using ActivateAudioInterfaceAsyncCallback =
      base::RepeatingCallback<HRESULT(LPCWSTR,
                                      REFIID,
                                      PROPVARIANT*,
                                      IActivateAudioInterfaceCompletionHandler*,
                                      IActivateAudioInterfaceAsyncOperation**)>;

  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIAudioInputStream(AudioManagerWin* manager,
                         const AudioParameters& params,
                         const std::string& device_id,
                         AudioManager::LogCallback log_callback);

  WASAPIAudioInputStream(const WASAPIAudioInputStream&) = delete;
  WASAPIAudioInputStream& operator=(const WASAPIAudioInputStream&) = delete;

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioInputStream::Close().
  ~WASAPIAudioInputStream() override;

  // Implementation of AudioInputStream.
  AudioInputStream::OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  bool started() const { return started_; }

  void SendLogMessage(std::string message);

  // Overrides the function pointer used to activate an IAudioClient during
  // application loopback captures. This is used for testing purposes only to
  // add a hook to obtain fake implementations of Windows interfaces.
  static void OverrideActivateAudioInterfaceAsyncCallbackForTesting(
      ActivateAudioInterfaceAsyncCallback callback);

  void OverrideAsyncActivationTimeoutForTesting(
      base::TimeDelta async_activation_timeout_ms) {
    async_activation_timeout_ms_ = async_activation_timeout_ms;
  }

 private:
  class DataDiscontinuityReporter;
  class EchoCancellationConfig;
  class AudioClientActivationHandler;

  PRINTF_FORMAT(2, 3) void SendLogMessage(const char* format, ...);

  // DelegateSimpleThread::Delegate implementation.
  void Run() override;

  // Pulls capture data from the endpoint device and pushes it to the sink.
  void PullCaptureDataAndPushToSink();

  // Issues the OnError() callback to the |sink_|.
  void HandleError(HRESULT err);

  // The Open() method is divided into these sub methods.
  HRESULT SetCaptureDevice();
  // Activates the IAudioClient interface with the adequate parameters. If
  // `device_id_` represents an application device, the function will call
  // ActivateAudioInterfaceAsync to activate an audio interface for process
  // loopback capture. If `device_id_` does not represent an application device,
  // it will activate the selected audio endpoint `endpoint_device_`.
  HRESULT ActivateAudioClientInterface();
  // Returns whether raw audio processing is supported or not for the selected
  // capture device.
  bool RawProcessingSupported();
  // Returns the native number of channels that the audio engine uses for its
  // internal processing of shared-mode streams.
  HRESULT GetAudioEngineNumChannels(WORD* channels);
  // Sets communications policy and excludes any built-in audio processing,
  // i.e., activates raw capture mode.
  // Raw capture mode is only enabled if the native number of input channels is
  // less than |media::kMaxConcurrentChannels| (8).
  HRESULT SetCommunicationsCategoryAndMaybeRawCaptureMode(WORD channels);
  // Returns whether the desired format is supported or not and writes the
  // result of a failing system call to |*hr|, or S_OK if successful. If this
  // function returns false with |*hr| == S_FALSE, the OS supports a closest
  // match but we don't support conversion to it.
  bool DesiredFormatIsSupported(HRESULT* hr);
  void SetupConverterAndStoreFormatInfo();
  HRESULT InitializeAudioEngine();

  void ReportOpenResult(HRESULT hr);
  // Reports stats for format related audio client initialization
  // (IAudioClient::Initialize) errors, that is if |hr| is an error related to
  // the format.
  void MaybeReportFormatRelatedInitError(HRESULT hr) const;

  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  // Reports glitch stats and resets associated variables.
  void ReportAndResetGlitchStats();

  // Creates the FIFO used to store audio data between the audio engine and the
  // converter.
  HRESULT CreateFifoIfNeeded();

  // Our creator, the audio manager needs to be notified when we close.
  const raw_ptr<AudioManagerWin> manager_;

  // Used to aggregate and report glitch metrics to UMA (periodically) and to
  // text logs (when a stream ends).
  SystemGlitchReporter glitch_reporter_;

  // Accumulates glitch info to be passed on to OnData().
  media::AudioGlitchInfo::Accumulator glitch_accumulator_;

  AmplitudePeakDetector peak_detector_;

  // Used to track and log data discontinuity warnings from
  // IAudioCaptureClient::GetBuffer.
  std::unique_ptr<DataDiscontinuityReporter> data_discontinuity_reporter_;

  // Capturing is driven by this thread (which has no message loop).
  // All OnData() callbacks will be called from this thread.
  std::unique_ptr<base::DelegateSimpleThread> capture_thread_;

  // Contains the desired output audio format which is set up at construction
  // and then never modified. It is the audio format this class will output
  // data to the sink in, or equivalently, the format after the converter if
  // such is needed. Does not need the extended version since we only support
  // max stereo at this stage.
  WAVEFORMATEX output_format_;

  // Contains the audio format we get data from the audio engine in. Initially
  // set to |output_format_| at construction but it might be changed to a close
  // match if the audio engine doesn't support the originally set format. Note
  // that, this is also the format after the FIFO, i.e. the input format to the
  // converter if any.
  WAVEFORMATEXTENSIBLE input_format_;

  bool opened_ = false;
  bool started_ = false;
  StreamOpenResult open_result_ = OPEN_RESULT_OK;

  // Size in bytes of each audio frame before the converter (e.g. 4 bytes for
  // 16-bit stereo PCM). Note that this is the same before and after the FIFO.
  size_t frame_size_bytes_ = 0;

  // Size in audio frames of each audio packet (buffer) after the FIFO but
  // before the converter.
  size_t packet_size_frames_ = 0;

  // Size in bytes of each audio packet (buffer) after the FIFO but before the
  // converter.
  size_t packet_size_bytes_ = 0;

  // Contains the unique name of the selected endpoint device.
  // Note that AudioDeviceDescription::kDefaultDeviceId represents the default
  // device role and is not a valid ID as such.
  std::string device_id_;

  // Pointer to the object that will receive the recorded audio samples.
  raw_ptr<AudioInputCallback> sink_ = nullptr;

  // Windows Multimedia Device (MMDevice) API interfaces.

  // An IMMDevice interface which represents an audio endpoint device.
  Microsoft::WRL::ComPtr<IMMDevice> endpoint_device_;

  // Windows Audio Session API (WASAPI) interfaces.

  // An IAudioClient interface which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;

  // Loopback IAudioClient supports event-driven mode but it requires an active
  // audio output. Some clients (e.g. Chromecast) needs to be able to deliver
  // a (silent) captured loopback stream even without active output audio, so a
  // separate IAudioClient is needed to receive notifications when data is
  // available in the buffer. For loopback input |audio_client_| is used to
  // receive data, while |audio_render_client_for_loopback_| is used as a helper
  // to get notifications when a new buffer is ready.
  // The extra rendering client is only created and used in combination
  // with endpoint devices or when |is_process_loopback_capture_| is false.
  // See comment inInitializeAudioEngine() for more details.
  Microsoft::WRL::ComPtr<IAudioClient> audio_render_client_for_loopback_;

  // The IAudioCaptureClient interface enables a client to read input data
  // from a capture endpoint buffer.
  Microsoft::WRL::ComPtr<IAudioCaptureClient> audio_capture_client_;

  // The ISimpleAudioVolume interface enables a client to control the
  // master volume level of an audio session.
  // The volume-level is a value in the range 0.0 to 1.0.
  // This interface does only work with shared-mode streams.
  Microsoft::WRL::ComPtr<ISimpleAudioVolume> simple_audio_volume_;

  // The IAudioEndpointVolume allows a client to control the volume level of
  // the whole system.
  Microsoft::WRL::ComPtr<IAudioEndpointVolume> system_audio_volume_;

  // The audio engine will signal this event each time a buffer has been
  // recorded.
  base::win::ScopedHandle audio_samples_ready_event_;

  // This event will be signaled when capturing shall stop.
  base::win::ScopedHandle stop_capture_event_;

  // Never set it through external API. Only used when |device_id_| ==
  // kLoopbackWithMuteDeviceId.
  // True, if we have muted the system audio for the stream capturing, and
  // indicates that we need to unmute the system audio when stopping capturing.
  bool mute_done_ = false;

  // Used to store data between the audio engine and the converter.
  std::unique_ptr<AudioBlockFifo> fifo_;

  // If the caller requires resampling (should only be in exceptional cases and
  // ideally, never), we support using an AudioConverter.
  std::unique_ptr<AudioConverter> converter_;
  std::unique_ptr<AudioBus> convert_bus_;
  bool imperfect_buffer_size_conversion_ = false;

  // Callback to send log messages to registered clients.
  AudioManager::LogCallback log_callback_;

  // For detecting and reporting glitches.
  UINT64 expected_next_device_position_ = 0;

  // Tracks error messages from IAudioCaptureClient::GetBuffer.
  UINT64 num_timestamp_errors_ = 0;
  base::TimeTicks record_start_time_;
  base::TimeDelta time_until_first_timestamp_error_;

  // Contains the last capture timestamp from IAudioCaptureClient::GetBuffer.
  base::TimeTicks last_capture_time_;

  // Max and min of difference in time between two successive timestamps.
  // |min_timestamp_diff_| should always be larger than or equal to one micro-
  // second.
  base::TimeDelta max_timestamp_diff_;
  base::TimeDelta min_timestamp_diff_;

  // Set to true if the selected audio device supports raw audio capture.
  // Also added to a UMA histogram.
  bool raw_processing_supported_ = false;

  // Set to true if the absolute difference between a QPC timestamp converted
  // into a TimeTick value and a default base::TimeTicks::Now() is larger than
  // 500 msec. A true return value should trigger usage of "fake" audio
  // timestamps instead of default which are QPC based.
  std::optional<bool> use_fake_audio_capture_timestamps_;

  // Utility class which wraps support of system AEC functionality.
  // Will be set to nullptr during construction if AEC is not supported.
  std::unique_ptr<EchoCancellationConfig> aec_config_;

  // Process loopback captures do not get audio from an endpoint device but
  // from a specified process IDs instead. It's is possible to check this using
  // an internal helper method called IsProcessLoopbackDevice. However, we need
  // to perform this check every time we need to pull data from the audio
  // engine, which can be expensive. Checking the variable is cheaper than
  // calling the function.
  const bool is_process_loopback_capture_;

  // Timeout period for waiting on the OS to activate the audio interface for
  // application loopback capture.
  base::TimeDelta async_activation_timeout_ms_ = base::Seconds(10);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_
