// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_output_win.h"

#include <Functiondiscoverykeys_devpkey.h>
#include <objbase.h>

#include <climits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_propvariant.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/audio_sample_types.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

using base::win::ScopedCOMInitializer;
using base::win::ScopedCoMem;

namespace media {

// static
AUDCLNT_SHAREMODE WASAPIAudioOutputStream::GetShareMode() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio))
    return AUDCLNT_SHAREMODE_EXCLUSIVE;
  return AUDCLNT_SHAREMODE_SHARED;
}

WASAPIAudioOutputStream::WASAPIAudioOutputStream(AudioManagerWin* manager,
                                                 const std::string& device_id,
                                                 const AudioParameters& params,
                                                 ERole device_role)
    : creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      format_(),
      opened_(false),
      volume_(1.0),
      packet_size_frames_(0),
      requested_iaudioclient3_buffer_size_(0),
      packet_size_bytes_(0),
      endpoint_buffer_size_frames_(0),
      device_id_(device_id),
      device_role_(device_role),
      share_mode_(GetShareMode()),
      num_written_frames_(0),
      source_(NULL) {
  DCHECK(manager_);

  // The empty string is used to indicate a default device and the
  // |device_role_| member controls whether that's the default or default
  // communications device.
  DCHECK_NE(device_id_, AudioDeviceDescription::kDefaultDeviceId);
  DCHECK_NE(device_id_, AudioDeviceDescription::kCommunicationsDeviceId);

  DVLOG(1) << "WASAPIAudioOutputStream::WASAPIAudioOutputStream()";
  DVLOG_IF(1, share_mode_ == AUDCLNT_SHAREMODE_EXCLUSIVE)
       << "Core Audio (WASAPI) EXCLUSIVE MODE is enabled.";
  DVLOG(1) << params.AsHumanReadableString();

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the avrt.dll";

  audio_bus_ = AudioBus::Create(params);

  // Set up the desired render format specified by the client. We use the
  // WAVE_FORMAT_EXTENSIBLE structure to ensure that multiple channel ordering
  // and high precision data can be supported.

  // Begin with the WAVEFORMATEX structure that specifies the basic format.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = params.channels();
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = sizeof(float) * 8;
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE.
  format_.Samples.wValidBitsPerSample = format->wBitsPerSample;
  format_.dwChannelMask = CoreAudioUtil::GetChannelConfig(device_id, eRender);
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  DVLOG(1) << "Format: " << CoreAudioUtil::WaveFormatToString(&format_);

  // Store size (in different units) of audio packets which we expect to
  // get from the audio endpoint device in each render event.
  packet_size_frames_ = params.frames_per_buffer();
  packet_size_bytes_ = params.GetBytesPerBuffer(kSampleFormatF32);
  DVLOG(1) << "Number of bytes per audio frame  : " << format->nBlockAlign;
  DVLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;
  DVLOG(1) << "Number of bytes per packet       : " << packet_size_bytes_;
  DVLOG(1) << "Number of milliseconds per packet: "
           << params.GetBufferDuration().InMillisecondsF();

  AudioParameters::HardwareCapabilities hardware_capabilities =
      params.hardware_capabilities().value_or(
          AudioParameters::HardwareCapabilities());

  // Only request an explicit buffer size if we are requesting the minimum
  // supported by the hardware, everything else uses the older IAudioClient API.
  if (params.frames_per_buffer() ==
      hardware_capabilities.min_frames_per_buffer) {
    requested_iaudioclient3_buffer_size_ =
        hardware_capabilities.min_frames_per_buffer;
  }

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_render_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_render_event_.IsValid());
}

WASAPIAudioOutputStream::~WASAPIAudioOutputStream() {
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
}

bool WASAPIAudioOutputStream::Open() {
  DVLOG(1) << "WASAPIAudioOutputStream::Open()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (opened_)
    return true;

  DCHECK(!audio_client_.Get());
  DCHECK(!audio_render_client_.Get());

  const bool communications_device =
      device_id_.empty() ? (device_role_ == eCommunications) : false;

  Microsoft::WRL::ComPtr<IAudioClient> audio_client(
      CoreAudioUtil::CreateClient(device_id_, eRender, device_role_));

  if (!audio_client.Get())
    return false;

  // Extra sanity to ensure that the provided device format is still valid.
  if (!CoreAudioUtil::IsFormatSupported(audio_client.Get(), share_mode_,
                                        &format_)) {
    LOG(ERROR) << "Audio parameters are not supported.";
    return false;
  }

  HRESULT hr = S_FALSE;
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // Initialize the audio stream between the client and the device in shared
    // mode and using event-driven buffer handling.
    hr = CoreAudioUtil::SharedModeInitialize(
        audio_client.Get(), &format_, audio_samples_render_event_.Get(),
        requested_iaudioclient3_buffer_size_, &endpoint_buffer_size_frames_,
        communications_device ? &kCommunicationsSessionId : NULL);
    if (FAILED(hr))
      return false;

    REFERENCE_TIME device_period = 0;
    if (FAILED(CoreAudioUtil::GetDevicePeriod(
            audio_client.Get(), AUDCLNT_SHAREMODE_SHARED, &device_period))) {
      return false;
    }

    const int preferred_frames_per_buffer = static_cast<int>(
        format_.Format.nSamplesPerSec *
            CoreAudioUtil::ReferenceTimeToTimeDelta(device_period)
                .InSecondsF() +
        0.5);

    // Packet size should always be an even divisor of the device period for
    // best performance; things will still work otherwise, but may glitch for a
    // couple of reasons.
    //
    // The first reason is if/when repeated RenderAudioFromSource() hit the
    // shared memory boundary between the renderer and the browser.  The next
    // audio buffer is always requested after the current request is consumed.
    // With back-to-back calls the round-trip may not be fast enough and thus
    // audio will glitch as we fail to deliver audio in a timely manner.
    //
    // The second reason is event wakeup efficiency.  We may have too few or too
    // many frames to fill the output buffer requested by WASAPI.  If too few,
    // we'll refuse the render event and wait until more output space is
    // available.  If we have too many frames, we'll only partially fill and
    // wait for the next render event.  In either case certain remainders may
    // leave us unable to fulfill the request in a timely manner, thus glitches.
    //
    // Log a warning in these cases so we can help users in the field.
    // Examples: 48kHz => 960 % 480, 44.1kHz => 896 % 448 or 882 % 441.
    if (preferred_frames_per_buffer % packet_size_frames_) {
      LOG(WARNING)
          << "Using WASAPI output with a non-optimal buffer size, glitches from"
          << " back to back shared memory reads and partial fills of WASAPI"
          << " output buffers may occur.  Buffer size of "
          << packet_size_frames_ << " is not an even divisor of "
          << preferred_frames_per_buffer;
    }
  } else {
    // TODO(henrika): break out to CoreAudioUtil::ExclusiveModeInitialize()
    // when removing the enable-exclusive-audio flag.
    hr = ExclusiveModeInitialization(audio_client.Get(),
                                     audio_samples_render_event_.Get(),
                                     &endpoint_buffer_size_frames_);
    if (FAILED(hr))
      return false;

    // The buffer scheme for exclusive mode streams is not designed for max
    // flexibility. We only allow a "perfect match" between the packet size set
    // by the user and the actual endpoint buffer size.
    if (endpoint_buffer_size_frames_ != packet_size_frames_) {
      LOG(ERROR) << "Bailing out due to non-perfect timing.";
      return false;
    }
  }

  // Create an IAudioRenderClient client for an initialized IAudioClient.
  // The IAudioRenderClient interface enables us to write output data to
  // a rendering endpoint buffer.
  Microsoft::WRL::ComPtr<IAudioRenderClient> audio_render_client =
      CoreAudioUtil::CreateRenderClient(audio_client.Get());
  if (!audio_render_client.Get())
    return false;

  // Store valid COM interfaces.
  audio_client_ = audio_client;
  audio_render_client_ = audio_render_client;

  hr = audio_client_->GetService(IID_PPV_ARGS(&audio_clock_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IAudioClock service.";
    return false;
  }

  opened_ = true;
  return true;
}

void WASAPIAudioOutputStream::Start(AudioSourceCallback* callback) {
  DVLOG(1) << "WASAPIAudioOutputStream::Start()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  CHECK(callback);
  CHECK(opened_);

  if (render_thread_) {
    CHECK_EQ(callback, source_);
    return;
  }

  // Ensure that the endpoint buffer is prepared with silence. Also serves as
  // a sanity check for the IAudioClient and IAudioRenderClient which may have
  // been invalidated by Windows since the last Stop() call.
  //
  // While technically we only need to retry when WASAPI tells us the device has
  // been invalidated (AUDCLNT_E_DEVICE_INVALIDATED), we retry for all errors
  // for simplicity and due to large sites like YouTube reporting high success
  // rates with a simple retry upon detection of an audio output error.
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    if (!CoreAudioUtil::FillRenderEndpointBufferWithSilence(
            audio_client_.Get(), audio_render_client_.Get())) {
      DLOG(WARNING) << "Failed to prepare endpoint buffers with silence. "
                       "Attempting recovery with a new IAudioClient and "
                       "IAudioRenderClient.";

      opened_ = false;
      audio_client_.Reset();
      audio_render_client_.Reset();
      if (!Open() || !CoreAudioUtil::FillRenderEndpointBufferWithSilence(
                         audio_client_.Get(), audio_render_client_.Get())) {
        DLOG(ERROR) << "Failed recovery of audio clients; Start() failed.";
        callback->OnError();
        return;
      }
    }
  }

  source_ = callback;
  num_written_frames_ = endpoint_buffer_size_frames_;
  last_position_ = 0;
  last_qpc_position_ = 0;

  // Create and start the thread that will drive the rendering by waiting for
  // render events.
  render_thread_.reset(new base::DelegateSimpleThread(
      this, "wasapi_render_thread",
      base::SimpleThread::Options(base::ThreadPriority::REALTIME_AUDIO)));
  render_thread_->Start();
  if (!render_thread_->HasBeenStarted()) {
    LOG(ERROR) << "Failed to start WASAPI render thread.";
    StopThread();
    callback->OnError();
    return;
  }

  // Start streaming data between the endpoint buffer and the audio engine.
  HRESULT hr = audio_client_->Start();
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to start output streaming: " << std::hex << hr;
    StopThread();
    callback->OnError();
  }
}

void WASAPIAudioOutputStream::Stop() {
  DVLOG(1) << "WASAPIAudioOutputStream::Stop()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (!render_thread_)
    return;

  // Stop output audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to stop output streaming: " << std::hex << hr;
    source_->OnError();
  }

  // Make a local copy of |source_| since StopThread() will clear it.
  AudioSourceCallback* callback = source_;
  StopThread();

  // Flush all pending data and reset the audio clock stream position to 0.
  hr = audio_client_->Reset();
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to reset streaming: " << std::hex << hr;
    callback->OnError();
  }

  ReportAndResetStats();

  // Extra safety check to ensure that the buffers are cleared.
  // If the buffers are not cleared correctly, the next call to Start()
  // would fail with AUDCLNT_E_BUFFER_ERROR at IAudioRenderClient::GetBuffer().
  // This check is is only needed for shared-mode streams.
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    UINT32 num_queued_frames = 0;
    audio_client_->GetCurrentPadding(&num_queued_frames);
    DCHECK_EQ(0u, num_queued_frames);
  }
}

void WASAPIAudioOutputStream::Close() {
  DVLOG(1) << "WASAPIAudioOutputStream::Close()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void WASAPIAudioOutputStream::Flush() {}

void WASAPIAudioOutputStream::SetVolume(double volume) {
  DVLOG(1) << "SetVolume(volume=" << volume << ")";
  float volume_float = static_cast<float>(volume);
  if (volume_float < 0.0f || volume_float > 1.0f) {
    return;
  }
  volume_ = volume_float;
}

void WASAPIAudioOutputStream::GetVolume(double* volume) {
  DVLOG(1) << "GetVolume()";
  *volume = static_cast<double>(volume_);
}

void WASAPIAudioOutputStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  DWORD task_index = 0;
  HANDLE mm_task = avrt::AvSetMmThreadCharacteristics(L"Pro Audio",
                                                      &task_index);
  bool mmcss_is_ok =
      (mm_task && avrt::AvSetMmThreadPriority(mm_task, AVRT_PRIORITY_CRITICAL));
  if (!mmcss_is_ok) {
    // Failed to enable MMCSS on this thread. It is not fatal but can lead
    // to reduced QoS at high load.
    DWORD err = GetLastError();
    LOG(WARNING) << "Failed to enable MMCSS (error code=" << err << ").";
  }

  HRESULT hr = S_FALSE;

  bool playing = true;
  bool error = false;
  HANDLE wait_array[] = { stop_render_event_.Get(),
                          audio_samples_render_event_.Get() };
  UINT64 device_frequency = 0;

  // The device frequency is the frequency generated by the hardware clock in
  // the audio device. The GetFrequency() method reports a constant frequency.
  hr = audio_clock_->GetFrequency(&device_frequency);
  error = FAILED(hr);
  PLOG_IF(ERROR, error) << "Failed to acquire IAudioClock interface: "
                        << std::hex << hr;

  // Keep rendering audio until the stop event or the stream-switch event
  // is signaled. An error event can also break the main thread loop.
  while (playing && !error) {
    // Wait for a close-down event, stream-switch event or a new render event.
    DWORD wait_result = WaitForMultipleObjects(base::size(wait_array),
                                               wait_array, FALSE, INFINITE);

    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_render_event_| has been set.
        playing = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |audio_samples_render_event_| has been set.
        error = !RenderAudioFromSource(device_frequency);
        break;
      default:
        error = true;
        break;
    }
  }

  if (playing && error) {
    LOG(ERROR) << "WASAPI rendering failed.";

    // Stop audio rendering since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    audio_client_->Stop();

    // Notify clients that something has gone wrong and that this stream should
    // be destroyed instead of reused in the future.
    source_->OnError();
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }
}

bool WASAPIAudioOutputStream::RenderAudioFromSource(UINT64 device_frequency) {
  TRACE_EVENT0("audio", "RenderAudioFromSource");

  HRESULT hr = S_FALSE;
  UINT32 num_queued_frames = 0;
  uint8_t* audio_data = NULL;

  // Contains how much new data we can write to the buffer without
  // the risk of overwriting previously written data that the audio
  // engine has not yet read from the buffer.
  size_t num_available_frames = 0;

  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // Get the padding value which represents the amount of rendering
    // data that is queued up to play in the endpoint buffer.
    hr = audio_client_->GetCurrentPadding(&num_queued_frames);
    num_available_frames =
        endpoint_buffer_size_frames_ - num_queued_frames;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to retrieve amount of available space: "
                  << std::hex << hr;
      return false;
    }
  } else {
    // While the stream is running, the system alternately sends one
    // buffer or the other to the client. This form of double buffering
    // is referred to as "ping-ponging". Each time the client receives
    // a buffer from the system (triggers this event) the client must
    // process the entire buffer. Calls to the GetCurrentPadding method
    // are unnecessary because the packet size must always equal the
    // buffer size. In contrast to the shared mode buffering scheme,
    // the latency for an event-driven, exclusive-mode stream depends
    // directly on the buffer size.
    num_available_frames = endpoint_buffer_size_frames_;
  }

  // Check if there is enough available space to fit the packet size
  // specified by the client.  If not, wait until a future callback.
  if (num_available_frames < packet_size_frames_)
    return true;

  // Derive the number of packets we need to get from the client to fill up the
  // available area in the endpoint buffer.  Well-behaved (> Vista) clients and
  // exclusive mode streams should generally have a |num_packets| value of 1.
  //
  // Vista clients are not able to maintain reliable callbacks, so the endpoint
  // buffer may exhaust itself such that back-to-back callbacks are occasionally
  // necessary to avoid glitches.  In such cases we have no choice but to issue
  // back-to-back reads and pray that the browser side has enough data cached or
  // that the render can fulfill the read before we glitch anyways.
  //
  // API documentation does not guarantee that even on Win7+ clients we won't
  // need to fill more than a period size worth of buffers; but in practice this
  // appears to be infrequent.
  //
  // See http://crbug.com/524947.
  const size_t num_packets = num_available_frames / packet_size_frames_;
  for (size_t n = 0; n < num_packets; ++n) {
    // Grab all available space in the rendering endpoint buffer
    // into which the client can write a data packet.
    hr = audio_render_client_->GetBuffer(packet_size_frames_,
                                         &audio_data);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to use rendering audio buffer: "
                 << std::hex << hr;
      return false;
    }

    // Derive the audio delay which corresponds to the delay between
    // a render event and the time when the first audio sample in a
    // packet is played out through the speaker. This delay value
    // can typically be utilized by an acoustic echo-control (AEC)
    // unit at the render side.
    UINT64 position = 0;
    UINT64 qpc_position = 0;
    base::TimeDelta delay;
    base::TimeTicks delay_timestamp;
    hr = audio_clock_->GetPosition(&position, &qpc_position);
    if (SUCCEEDED(hr)) {
      // Number of frames already played out through the speaker.
      const uint64_t played_out_frames =
          format_.Format.nSamplesPerSec * position / device_frequency;

      // Check for glitches. Records a glitch whenever the stream's position has
      // moved forward significantly less than the performance counter has. The
      // threshold is set to half the buffer size, to limit false positives.
      if (last_qpc_position_ != 0) {
        const int64_t buffer_duration_us = packet_size_frames_ *
                                           base::Time::kMicrosecondsPerSecond /
                                           format_.Format.nSamplesPerSec;

        const int64_t position_us =
            position * base::Time::kMicrosecondsPerSecond / device_frequency;
        const int64_t last_position_us = last_position_ *
                                         base::Time::kMicrosecondsPerSecond /
                                         device_frequency;
        // The QPC values are in 100 ns units.
        const int64_t qpc_position_us = qpc_position / 10;
        const int64_t last_qpc_position_us = last_qpc_position_ / 10;

        const int64_t position_diff_us = position_us - last_position_us;
        const int64_t qpc_position_diff_us =
            qpc_position_us - last_qpc_position_us;

        if (qpc_position_diff_us - position_diff_us > buffer_duration_us / 2) {
          ++num_glitches_detected_;

          base::TimeDelta glitch_duration = base::TimeDelta::FromMicroseconds(
              qpc_position_diff_us - position_diff_us);

          if (glitch_duration > largest_glitch_)
            largest_glitch_ = glitch_duration;

          cumulative_audio_lost_ += glitch_duration;
        }
      }

      last_position_ = position;
      last_qpc_position_ = qpc_position;

      // Number of frames that have been written to the buffer but not yet
      // played out.
      const uint64_t delay_frames = num_written_frames_ - played_out_frames;

      // Convert the delay from frames to time.
      delay = base::TimeDelta::FromMicroseconds(
          delay_frames * base::Time::kMicrosecondsPerSecond /
          format_.Format.nSamplesPerSec);
      // Note: the obtained |qpc_position| value is in 100ns intervals and from
      // the same time origin as QPC. We can simply convert it into us dividing
      // by 10.0 since 10x100ns = 1us.
      delay_timestamp += base::TimeDelta::FromMicroseconds(qpc_position * 0.1);
    } else {
      // Use a delay of zero.
      delay_timestamp = base::TimeTicks::Now();
    }

    // Read a data packet from the registered client source and
    // deliver a delay estimate in the same callback to the client.

    int frames_filled =
        source_->OnMoreData(delay, delay_timestamp, 0, audio_bus_.get());
    uint32_t num_filled_bytes = frames_filled * format_.Format.nBlockAlign;
    DCHECK_LE(num_filled_bytes, packet_size_bytes_);

    audio_bus_->Scale(volume_);

    // We skip clipping since that occurs at the shared memory boundary.
    audio_bus_->ToInterleaved<Float32SampleTypeTraitsNoClip>(
        frames_filled, reinterpret_cast<float*>(audio_data));

    // Release the buffer space acquired in the GetBuffer() call.
    // Render silence if we were not able to fill up the buffer totally.
    DWORD flags = (num_filled_bytes < packet_size_bytes_) ?
        AUDCLNT_BUFFERFLAGS_SILENT : 0;
    audio_render_client_->ReleaseBuffer(packet_size_frames_, flags);

    num_written_frames_ += packet_size_frames_;
  }

  return true;
}

HRESULT WASAPIAudioOutputStream::ExclusiveModeInitialization(
    IAudioClient* client,
    HANDLE event_handle,
    uint32_t* endpoint_buffer_size) {
  DCHECK_EQ(share_mode_, AUDCLNT_SHAREMODE_EXCLUSIVE);

  float f = (1000.0 * packet_size_frames_) / format_.Format.nSamplesPerSec;
  REFERENCE_TIME requested_buffer_duration =
      static_cast<REFERENCE_TIME>(f * 10000.0 + 0.5);

  DWORD stream_flags = AUDCLNT_STREAMFLAGS_NOPERSIST;
  bool use_event = (event_handle != NULL &&
                    event_handle != INVALID_HANDLE_VALUE);
  if (use_event)
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  DVLOG(2) << "stream_flags: 0x" << std::hex << stream_flags;

  // Initialize the audio stream between the client and the device.
  // For an exclusive-mode stream that uses event-driven buffering, the
  // caller must specify nonzero values for hnsPeriodicity and
  // hnsBufferDuration, and the values of these two parameters must be equal.
  // The Initialize method allocates two buffers for the stream. Each buffer
  // is equal in duration to the value of the hnsBufferDuration parameter.
  // Following the Initialize call for a rendering stream, the caller should
  // fill the first of the two buffers before starting the stream.
  HRESULT hr = S_FALSE;
  hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                          stream_flags,
                          requested_buffer_duration,
                          requested_buffer_duration,
                          reinterpret_cast<WAVEFORMATEX*>(&format_),
                          NULL);
  if (FAILED(hr)) {
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
      LOG(ERROR) << "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";

      UINT32 aligned_buffer_size = 0;
      client->GetBufferSize(&aligned_buffer_size);
      DVLOG(1) << "Use aligned buffer size instead: " << aligned_buffer_size;

      // Calculate new aligned periodicity. Each unit of reference time
      // is 100 nanoseconds.
      REFERENCE_TIME aligned_buffer_duration = static_cast<REFERENCE_TIME>(
          (10000000.0 * aligned_buffer_size / format_.Format.nSamplesPerSec)
          + 0.5);

      // It is possible to re-activate and re-initialize the audio client
      // at this stage but we bail out with an error code instead and
      // combine it with a log message which informs about the suggested
      // aligned buffer size which should be used instead.
      DVLOG(1) << "aligned_buffer_duration: "
               << static_cast<double>(aligned_buffer_duration / 10000.0)
               << " [ms]";
    } else if (hr == AUDCLNT_E_INVALID_DEVICE_PERIOD) {
      // We will get this error if we try to use a smaller buffer size than
      // the minimum supported size (usually ~3ms on Windows 7).
      LOG(ERROR) << "AUDCLNT_E_INVALID_DEVICE_PERIOD";
    }
    return hr;
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
  return hr;
}

void WASAPIAudioOutputStream::StopThread() {
  if (render_thread_) {
    if (render_thread_->HasBeenStarted()) {
      // Wait until the thread completes and perform cleanup.
      SetEvent(stop_render_event_.Get());
      render_thread_->Join();
    }

    render_thread_.reset();

    // Ensure that we don't quit the main thread loop immediately next
    // time Start() is called.
    ResetEvent(stop_render_event_.Get());
  }

  source_ = NULL;
}

void WASAPIAudioOutputStream::ReportAndResetStats() {
  // Even if there aren't any glitches, we want to record it to get a feel for
  // how often we get no glitches vs the alternative.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Media.Audio.Render.Glitches",
                              num_glitches_detected_, 1, 999999, 100);
  // Don't record these unless there actually was a glitch, though.
  if (num_glitches_detected_ != 0) {
    UMA_HISTOGRAM_COUNTS_1M("Media.Audio.Render.LostFramesInMs",
                            cumulative_audio_lost_.InMilliseconds());
    UMA_HISTOGRAM_COUNTS_1M("Media.Audio.Render.LargestGlitchMs",
                            largest_glitch_.InMilliseconds());
  }
  num_glitches_detected_ = 0;
  cumulative_audio_lost_ = base::TimeDelta();
  largest_glitch_ = base::TimeDelta();
}

}  // namespace media
