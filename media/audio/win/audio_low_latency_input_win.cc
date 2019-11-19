// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_input_win.h"

#include <objbase.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"

using base::win::ScopedCOMInitializer;

namespace media {

namespace {

constexpr uint32_t KSAUDIO_SPEAKER_UNSUPPORTED = 0;

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

}  // namespace

WASAPIAudioInputStream::WASAPIAudioInputStream(
    AudioManagerWin* manager,
    const AudioParameters& params,
    const std::string& device_id,
    const AudioManager::LogCallback& log_callback)
    : manager_(manager), device_id_(device_id), log_callback_(log_callback) {
  DCHECK(manager_);
  DCHECK(!device_id_.empty());
  DCHECK(!log_callback_.is_null());
  DCHECK_LE(params.channels(), 2);
  DCHECK(params.channel_layout() == CHANNEL_LAYOUT_MONO ||
         params.channel_layout() == CHANNEL_LAYOUT_STEREO ||
         params.channel_layout() == CHANNEL_LAYOUT_DISCRETE);

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the Avrt.dll";

  const SampleFormat kSampleFormat = kSampleFormatS16;

  // The clients asks for an input stream specified by |params|. Start by
  // setting up an input device format according to the same specification.
  // If all goes well during the upcoming initialization, this format will not
  // change. However, under some circumstances, minor changes can be required
  // to fit the current input audio device. If so, a FIFO and/or and audio
  // converter might be needed to ensure that the output format of this stream
  // matches what the client asks for.
  DVLOG(1) << params.AsHumanReadableString();
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
  DVLOG(1) << "Input: " << CoreAudioUtil::WaveFormatToString(&input_format_);

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
  DVLOG(1) << "Output: " << CoreAudioUtil::WaveFormatToString(&output_format_);

  // Size in bytes of each audio frame.
  frame_size_bytes_ = format->nBlockAlign;

  // Store size of audio packets which we expect to get from the audio
  // endpoint device in each capture event.
  packet_size_bytes_ = params.GetBytesPerBuffer(kSampleFormat);
  packet_size_frames_ = packet_size_bytes_ / format->nBlockAlign;
  DVLOG(1) << "Number of bytes per audio frame  : " << frame_size_bytes_;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;

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

bool WASAPIAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Verify that we are not already opened.
  if (opened_) {
    log_callback_.Run("WASAPIAIS::Open: already open");
    return false;
  }

  // Obtain a reference to the IMMDevice interface of the capturing
  // device with the specified unique identifier or role which was
  // set at construction.
  HRESULT hr = SetCaptureDevice();
  if (FAILED(hr)) {
    ReportOpenResult(hr);
    return false;
  }

  // Obtain an IAudioClient interface which enables us to create and initialize
  // an audio stream between an audio application and the audio engine.
  hr = endpoint_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  &audio_client_);
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_ACTIVATION_FAILED;
    ReportOpenResult(hr);
    return false;
  }

#ifndef NDEBUG
  // Retrieve the stream format which the audio engine uses for its internal
  // processing/mixing of shared-mode streams. This function call is for
  // diagnostic purposes only and only in debug mode.
  hr = GetAudioEngineStreamFormat();
#endif

  // Verify that the selected audio endpoint supports the specified format
  // set during construction.
  hr = S_OK;
  if (!DesiredFormatIsSupported(&hr)) {
    open_result_ = OPEN_RESULT_FORMAT_NOT_SUPPORTED;
    ReportOpenResult(hr);
    return false;
  }

  // Initialize the audio stream between the client and the device using
  // shared mode and a lowest possible glitch-free latency.
  hr = InitializeAudioEngine();
  if (SUCCEEDED(hr) && converter_)
    open_result_ = OPEN_RESULT_OK_WITH_RESAMPLING;
  ReportOpenResult(hr);  // Report before we assign a value to |opened_|.
  opened_ = SUCCEEDED(hr);

  return opened_;
}

void WASAPIAudioInputStream::Start(AudioInputCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return;

  if (started_)
    return;

  // Check if the master volume level of the opened audio session is set to
  // zero and store the information for a UMA histogram generated in Stop().
  // Valid volume levels are in the range 0.0 to 1.0.
  // See http://crbug.com/1014443 for details why this is needed.
  if (GetVolume() == 0.0) {
    DLOG(WARNING) << "Input audio session starts with zero volume";
    audio_session_starts_at_zero_volume_ = true;
  }

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
  capture_thread_.reset(new base::DelegateSimpleThread(
      this, "wasapi_capture_thread",
      base::SimpleThread::Options(base::ThreadPriority::REALTIME_AUDIO)));
  capture_thread_->Start();

  // Start streaming data between the endpoint buffer and the audio engine.
  HRESULT hr = audio_client_->Start();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to start input streaming.";
    log_callback_.Run(base::StringPrintf(
        "WASAPIAIS::Start: Failed to start audio client, hresult = %#lx", hr));
  }

  if (SUCCEEDED(hr) && audio_render_client_for_loopback_.Get()) {
    hr = audio_render_client_for_loopback_->Start();
    if (FAILED(hr))
      log_callback_.Run(base::StringPrintf(
          "WASAPIAIS::Start: Failed to start render client for loopback, "
          "hresult = %#lx",
          hr));
  }

  started_ = SUCCEEDED(hr);
}

void WASAPIAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "WASAPIAudioInputStream::Stop()";
  if (!started_)
    return;

  // Only upload UMA histogram for the case when AGC is enabled, i.e., for
  // WebRTC based audio input streams.
  const bool add_uma_histogram = GetAutomaticGainControl();

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
    LOG(ERROR) << "Failed to stop input streaming.";
  }

  // Wait until the thread completes and perform cleanup.
  if (capture_thread_) {
    SetEvent(stop_capture_event_.Get());
    capture_thread_->Join();
    capture_thread_.reset();
  }

  // Upload UMA histogram to track down possible issue that can lead to a
  // "no audio" state. See http://crbug.com/1014443.
  if (add_uma_histogram) {
    base::UmaHistogramBoolean("Media.Audio.InputVolumeStartsAtZeroWin",
                              audio_session_starts_at_zero_volume_);
    DVLOG(1) << "Media.Audio.InputVolumeStartsAtZeroWin: "
             << audio_session_starts_at_zero_volume_;
    audio_session_starts_at_zero_volume_ = false;
  }

  started_ = false;
  sink_ = nullptr;
}

void WASAPIAudioInputStream::Close() {
  DVLOG(1) << "WASAPIAudioInputStream::Close()";
  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  if (converter_)
    converter_->RemoveInput(this);

  ReportAndResetGlitchStats();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseInputStream(this);
}

double WASAPIAudioInputStream::GetMaxVolume() {
  // Verify that Open() has been called succesfully, to ensure that an audio
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

  DLOG_IF(ERROR, !opened_) << "Open() has not been called successfully";
  if (!opened_)
    return;

  // Set a new master volume level. Valid volume levels are in the range
  // 0.0 to 1.0. Ignore volume-change events.
  HRESULT hr = simple_audio_volume_->SetMasterVolume(static_cast<float>(volume),
                                                     nullptr);
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to set new input master volume.";

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
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to get input master volume.";

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
  if (FAILED(hr))
    DLOG(WARNING) << "Failed to get input master volume.";

  return is_muted != FALSE;
}

void WASAPIAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
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
    LOG(WARNING) << "Failed to enable MMCSS (error code=" << err << ").";
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
  fifo_.reset(new AudioBlockFifo(input_format_.Format.nChannels,
                                 packet_size_frames_, buffers_required));

  DVLOG(1) << "AudioBlockFifo buffer count: " << buffers_required;

  bool recording = true;
  bool error = false;
  HANDLE wait_array[2] = {stop_capture_event_.Get(),
                          audio_samples_ready_event_.Get()};

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
    NOTREACHED() << "WASAPI capturing failed with error code "
                 << GetLastError();
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

  // Pull data from the capture endpoint buffer until it's empty or an error
  // occurs.
  while (true) {
    BYTE* data_ptr = nullptr;
    UINT32 num_frames_to_read = 0;
    DWORD flags = 0;
    UINT64 device_position = 0;

    // Note: The units on this are 100ns intervals. Both GetBuffer() and
    // GetPosition() will handle the translation from the QPC value, so we just
    // need to convert from 100ns units into us. Which is just dividing by 10.0
    // since 10x100ns = 1us.
    UINT64 capture_time_100ns = 0;

    // Retrieve the amount of data in the capture endpoint buffer, replace it
    // with silence if required, create callbacks for each packet and store
    // non-delivered data for the next event.
    HRESULT hr =
        audio_capture_client_->GetBuffer(&data_ptr, &num_frames_to_read, &flags,
                                         &device_position, &capture_time_100ns);
    if (hr == AUDCLNT_S_BUFFER_EMPTY)
      break;

    // TODO(grunell): Should we handle different errors explicitly? Perhaps exit
    // by setting |error = true|. What are the assumptions here that makes us
    // rely on the next WaitForMultipleObjects? Do we expect the next wait to be
    // successful sometimes?
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get data from the capture buffer";
      break;
    }

    // If the device position has changed, we assume this data belongs to a new
    // chunk, so we report delay and glitch stats and update the last and next
    // expected device positions.
    // If the device position has not changed we assume this data belongs to the
    // previous chunk, and only update the expected next device position.
    if (device_position != last_device_position) {
      UpdateGlitchCount(device_position);
      last_device_position = device_position;
      expected_next_device_position_ = device_position + num_frames_to_read;
    } else {
      expected_next_device_position_ += num_frames_to_read;
    }

    // TODO(dalecurtis, olka, grunell): Is this ever false? If it is, should we
    // handle |flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR|?
    if (audio_clock_) {
      // The reported timestamp from GetBuffer is not as reliable as the clock
      // from the client.  We've seen timestamps reported for USB audio devices,
      // be off by several days.  Furthermore we've seen them jump back in time
      // every 2 seconds or so.
      // TODO(grunell): Using the audio clock as capture time for the currently
      // processed buffer seems incorrect. http://crbug.com/825744.
      audio_clock_->GetPosition(&device_position, &capture_time_100ns);
    }

    base::TimeTicks capture_time;
    if (capture_time_100ns) {
      // See conversion notes on |capture_time_100ns|.
      capture_time +=
          base::TimeDelta::FromMicroseconds(capture_time_100ns / 10.0);
    } else {
      // We may not have an IAudioClock or GetPosition() may return zero.
      capture_time = base::TimeTicks::Now();
    }

    // Adjust |capture_time| for the FIFO before pushing.
    capture_time -= AudioTimestampHelper::FramesToTime(
        fifo_->GetAvailableFrames(), input_format_.Format.nSamplesPerSec);

    // TODO(grunell): Since we check |hr == AUDCLNT_S_BUFFER_EMPTY| above,
    // should we instead assert that |num_frames_to_read != 0|?
    if (num_frames_to_read != 0) {
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        fifo_->PushSilence(num_frames_to_read);
      } else {
        fifo_->Push(data_ptr, num_frames_to_read,
                    input_format_.Format.wBitsPerSample / 8);
      }
    }

    hr = audio_capture_client_->ReleaseBuffer(num_frames_to_read);
    DLOG_IF(ERROR, FAILED(hr)) << "Failed to release capture buffer";

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
          break;
        }
        converter_->Convert(convert_bus_.get());
        sink_->OnData(convert_bus_.get(), capture_time, volume);

        // Move the capture time forward for each vended block.
        capture_time += AudioTimestampHelper::FramesToTime(
            convert_bus_->frames(), output_format_.nSamplesPerSec);
      } else {
        sink_->OnData(fifo_->Consume(), capture_time, volume);

        // Move the capture time forward for each vended block.
        capture_time += AudioTimestampHelper::FramesToTime(
            packet_size_frames_, input_format_.Format.nSamplesPerSec);
      }
    }
  }  // while (true)
}

void WASAPIAudioInputStream::HandleError(HRESULT err) {
  NOTREACHED() << "Error code: " << err;
  if (sink_)
    sink_->OnError();
}

HRESULT WASAPIAudioInputStream::SetCaptureDevice() {
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);
  DCHECK(!endpoint_device_.Get());

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
    hr = enumerator->GetDevice(base::UTF8ToUTF16(device_id_).c_str(),
                               endpoint_device_.GetAddressOf());
  }
  if (FAILED(hr)) {
    open_result_ = OPEN_RESULT_NO_ENDPOINT;
    return hr;
  }

  // If loopback device with muted system audio is requested, get the volume
  // interface for the endpoint.
  if (device_id_ == AudioDeviceDescription::kLoopbackWithMuteDeviceId) {
    hr = endpoint_device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                    nullptr, &system_audio_volume_);
    if (FAILED(hr)) {
      open_result_ = OPEN_RESULT_ACTIVATION_FAILED;
      return hr;
    }
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

HRESULT WASAPIAudioInputStream::GetAudioEngineStreamFormat() {
  HRESULT hr = S_OK;
#ifndef NDEBUG
  base::win::ScopedCoMem<WAVEFORMATEX> format;
  hr = audio_client_->GetMixFormat(&format);
  if (FAILED(hr))
    return hr;
  DVLOG(1) << CoreAudioUtil::WaveFormatToString(format.get());
#endif
  return hr;
}

bool WASAPIAudioInputStream::DesiredFormatIsSupported(HRESULT* hr) {
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
  DLOG_IF(ERROR, hresult == S_FALSE)
      << "Format is not supported but a closest match exists.";
  if (FAILED(hresult))
    LOG(ERROR) << "Input format is not supported: " << std::hex << hresult;

  if (hresult == S_FALSE) {
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
      DVLOG(1) << "Will convert captured audio: \n"
               << CoreAudioUtil::WaveFormatToString(&input_format_) << " ==> \n"
               << CoreAudioUtil::WaveFormatToString(&output_format_);

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

  const auto input_layout = GuessChannelLayout(input_format_.Format.nChannels);
  DCHECK_NE(CHANNEL_LAYOUT_UNSUPPORTED, input_layout);
  const auto output_layout = GuessChannelLayout(output_format_.nChannels);
  DCHECK_NE(CHANNEL_LAYOUT_UNSUPPORTED, output_layout);

  const AudioParameters input(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                              input_layout, input_format_.Format.nSamplesPerSec,
                              static_cast<int>(new_frames_per_buffer));

  const AudioParameters output(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               output_layout, output_format_.nSamplesPerSec,
                               packet_size_frames_);

  converter_.reset(new AudioConverter(input, output, false));
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
  DVLOG_IF(1, imperfect_buffer_size_conversion_)
      << "Audio capture data conversion: Need to inject fifo";
}

HRESULT WASAPIAudioInputStream::InitializeAudioEngine() {
  DCHECK_EQ(OPEN_RESULT_OK, open_result_);
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
  DVLOG(1) << "Audio format used in IAudioClient::Initialize: "
           << CoreAudioUtil::WaveFormatToString(&input_format_);
  HRESULT hr = audio_client_->Initialize(
      AUDCLNT_SHAREMODE_SHARED, flags,
      100 * 1000 * 10,  // Buffer duration, 100 ms expressed in 100-ns units.
      0,                // Device period, n/a for shared mode.
      reinterpret_cast<const WAVEFORMATEX*>(&input_format_),
      AudioDeviceDescription::IsCommunicationsDevice(device_id_)
          ? &kCommunicationsSessionId
          : nullptr);

  if (FAILED(hr)) {
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

#ifndef NDEBUG
  const int endpoint_buffer_size_ms =
      static_cast<double>(endpoint_buffer_size_frames_ * 1000) /
          input_format_.Format.nSamplesPerSec +
      0.5;  // Round to closest integer
  DVLOG(1) << "Endpoint buffer size: " << endpoint_buffer_size_frames_
           << " frames (" << endpoint_buffer_size_ms << " ms)";

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

  audio_client_->GetService(IID_PPV_ARGS(&audio_clock_));
  if (!audio_clock_)
    LOG(WARNING) << "IAudioClock unavailable, capture times may be inaccurate.";

  return hr;
}

void WASAPIAudioInputStream::ReportOpenResult(HRESULT hr) const {
  DCHECK(!opened_);  // This method must be called before we set this flag.
  UMA_HISTOGRAM_ENUMERATION("Media.Audio.Capture.Win.Open", open_result_,
                            OPEN_RESULT_MAX + 1);
  if (open_result_ != OPEN_RESULT_OK &&
      open_result_ != OPEN_RESULT_OK_WITH_RESAMPLING) {
    log_callback_.Run(base::StringPrintf(
        "WASAPIAIS::Open: failed, result = %d, hresult = %#lx, "
        "input format = %#x/%d/%ld/%d/%d/%ld/%d, "
        "output format = %#x/%d/%ld/%d/%d/%ld/%d",
        // clang-format off
        open_result_, hr,
        input_format_.Format.wFormatTag, input_format_.Format.nChannels,
        input_format_.Format.nSamplesPerSec,
        input_format_.Format.wBitsPerSample,
        input_format_.Format.nBlockAlign, input_format_.Format.nAvgBytesPerSec,
        input_format_.Format.cbSize,
        output_format_.wFormatTag, output_format_.nChannels,
        output_format_.nSamplesPerSec,
        output_format_.wBitsPerSample,
        output_format_.nBlockAlign,
        output_format_.nAvgBytesPerSec,
        output_format_.cbSize));
    // clang-format on
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

double WASAPIAudioInputStream::ProvideInput(AudioBus* audio_bus,
                                            uint32_t frames_delayed) {
  fifo_->Consume()->CopyTo(audio_bus);
  return 1.0;
}

void WASAPIAudioInputStream::UpdateGlitchCount(UINT64 device_position) {
  if (expected_next_device_position_ != 0) {
    if (device_position > expected_next_device_position_) {
      ++total_glitches_;
      auto lost_frames = device_position - expected_next_device_position_;
      total_lost_frames_ += lost_frames;
      if (lost_frames > largest_glitch_frames_)
        largest_glitch_frames_ = lost_frames;
    }
  }
}

void WASAPIAudioInputStream::ReportAndResetGlitchStats() {
  UMA_HISTOGRAM_COUNTS_1M("Media.Audio.Capture.Glitches", total_glitches_);

  double lost_frames_ms =
      (total_lost_frames_ * 1000) / input_format_.Format.nSamplesPerSec;
  std::string log_message = base::StringPrintf(
      "WASAPIAIS: Total glitches=%d. Total frames lost=%llu (%.0lf ms).",
      total_glitches_, total_lost_frames_, lost_frames_ms);
  log_callback_.Run(log_message);

  if (total_glitches_ != 0) {
    UMA_HISTOGRAM_LONG_TIMES("Media.Audio.Capture.LostFramesInMs",
                             base::TimeDelta::FromMilliseconds(lost_frames_ms));
    int64_t largest_glitch_ms =
        (largest_glitch_frames_ * 1000) / input_format_.Format.nSamplesPerSec;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.Audio.Capture.LargestGlitchMs",
        base::TimeDelta::FromMilliseconds(largest_glitch_ms),
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
        50);
    DLOG(WARNING) << log_message;
  }

  expected_next_device_position_ = 0;
  total_glitches_ = 0;
  total_lost_frames_ = 0;
  largest_glitch_frames_ = 0;
}

}  // namespace media
