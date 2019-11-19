// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include <Audioclient.h>
#include <MMDeviceAPI.h>
#include <endpointvolume.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "media/audio/agc_audio_stream.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/base/audio_converter.h"
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
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIAudioInputStream(AudioManagerWin* manager,
                         const AudioParameters& params,
                         const std::string& device_id,
                         const AudioManager::LogCallback& log_callback);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioInputStream::Close().
  ~WASAPIAudioInputStream() override;

  // Implementation of AudioInputStream.
  bool Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  bool started() const { return started_; }

 private:
  // DelegateSimpleThread::Delegate implementation.
  void Run() override;

  // Pulls capture data from the endpoint device and pushes it to the sink.
  void PullCaptureDataAndPushToSink();

  // Issues the OnError() callback to the |sink_|.
  void HandleError(HRESULT err);

  // The Open() method is divided into these sub methods.
  HRESULT SetCaptureDevice();
  HRESULT GetAudioEngineStreamFormat();
  // Returns whether the desired format is supported or not and writes the
  // result of a failing system call to |*hr|, or S_OK if successful. If this
  // function returns false with |*hr| == S_FALSE, the OS supports a closest
  // match but we don't support conversion to it.
  bool DesiredFormatIsSupported(HRESULT* hr);
  void SetupConverterAndStoreFormatInfo();
  HRESULT InitializeAudioEngine();
  void ReportOpenResult(HRESULT hr) const;
  // Reports stats for format related audio client initilization
  // (IAudioClient::Initialize) errors, that is if |hr| is an error related to
  // the format.
  void MaybeReportFormatRelatedInitError(HRESULT hr) const;

  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override;

  // Detects and counts glitches based on |device_position|.
  void UpdateGlitchCount(UINT64 device_position);

  // Reports glitch stats and resets associated variables.
  void ReportAndResetGlitchStats();

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
    OPEN_RESULT_GET_BUFFER_SIZE_FAILED = 8,
    OPEN_RESULT_LOOPBACK_ACTIVATE_FAILED = 9,
    OPEN_RESULT_LOOPBACK_INIT_FAILED = 10,
    OPEN_RESULT_SET_EVENT_HANDLE = 11,
    OPEN_RESULT_NO_CAPTURE_CLIENT = 12,
    OPEN_RESULT_NO_AUDIO_VOLUME = 13,
    OPEN_RESULT_OK_WITH_RESAMPLING = 14,
    OPEN_RESULT_MAX = OPEN_RESULT_OK_WITH_RESAMPLING
  };

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerWin* const manager_;

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

  // Size in bytes of each audio frame before the converter (4 bytes for 16-bit
  // stereo PCM). Note that this is the same before and after the fifo.
  size_t frame_size_bytes_ = 0;

  // Size in audio frames of each audio packet (buffer) after the fifo but
  // before the converter.
  size_t packet_size_frames_ = 0;

  // Size in bytes of each audio packet (buffer) after the fifo but before the
  // converter.
  size_t packet_size_bytes_ = 0;

  // Length of the audio endpoint buffer, i.e. the buffer size before the fifo.
  uint32_t endpoint_buffer_size_frames_ = 0;

  // Contains the unique name of the selected endpoint device.
  // Note that AudioDeviceDescription::kDefaultDeviceId represents the default
  // device role and is not a valid ID as such.
  std::string device_id_;

  // Pointer to the object that will receive the recorded audio samples.
  AudioInputCallback* sink_ = nullptr;

  // Windows Multimedia Device (MMDevice) API interfaces.

  // An IMMDevice interface which represents an audio endpoint device.
  Microsoft::WRL::ComPtr<IMMDevice> endpoint_device_;

  // Windows Audio Session API (WASAPI) interfaces.

  // An IAudioClient interface which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;

  // Loopback IAudioClient doesn't support event-driven mode, so a separate
  // IAudioClient is needed to receive notifications when data is available in
  // the buffer. For loopback input |audio_client_| is used to receive data,
  // while |audio_render_client_for_loopback_| is used to get notifications
  // when a new buffer is ready. See comment in InitializeAudioEngine() for
  // details.
  Microsoft::WRL::ComPtr<IAudioClient> audio_render_client_for_loopback_;

  // The IAudioCaptureClient interface enables a client to read input data
  // from a capture endpoint buffer.
  Microsoft::WRL::ComPtr<IAudioCaptureClient> audio_capture_client_;

  // The IAudioClock interface is used to get the current timestamp, as the
  // timestamp from IAudioCaptureClient::GetBuffer can be unreliable with some
  // devices.
  Microsoft::WRL::ComPtr<IAudioClock> audio_clock_;

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

  // Used for the captured audio on the callback thread.
  std::unique_ptr<AudioBlockFifo> fifo_;

  // If the caller requires resampling (should only be in exceptional cases and
  // ideally, never), we support using an AudioConverter.
  std::unique_ptr<AudioConverter> converter_;
  std::unique_ptr<AudioBus> convert_bus_;
  bool imperfect_buffer_size_conversion_ = false;

  // Callback to send log messages.
  AudioManager::LogCallback log_callback_;

  // For detecting and reporting glitches.
  UINT64 expected_next_device_position_ = 0;
  int total_glitches_ = 0;
  UINT64 total_lost_frames_ = 0;
  UINT64 largest_glitch_frames_ = 0;

  // Enabled if the volume level of the audio session is set to zero when the
  // session starts. Utilized in UMA histogram.
  bool audio_session_starts_at_zero_volume_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WASAPIAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_
