// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of AudioOutputStream for Windows using Windows Core Audio
// WASAPI for low latency rendering.
//
// Overview of operation and performance:
//
// - An object of WASAPIAudioOutputStream is created by the AudioManager
//   factory.
// - Next some thread will call Open(), at that point the underlying
//   Core Audio APIs are utilized to create two WASAPI interfaces called
//   IAudioClient and IAudioRenderClient.
// - Then some thread will call Start(source).
//   A thread called "wasapi_render_thread" is started and this thread listens
//   on an event signal which is set periodically by the audio engine to signal
//   render events. As a result, OnMoreData() will be called and the registered
//   client is then expected to provide data samples to be played out.
// - At some point, a thread will call Stop(), which stops and joins the
//   render thread and at the same time stops audio streaming.
// - The same thread that called stop will call Close() where we cleanup
//   and notify the audio manager, which likely will destroy this object.
// - A total typical delay of 35 ms contains three parts:
//    o Audio endpoint device period (~10 ms).
//    o Stream latency between the buffer and endpoint device (~5 ms).
//    o Endpoint buffer (~20 ms to ensure glitch-free rendering).
//
// Implementation notes:
//
// - The minimum supported client is Windows Vista.
// - This implementation is single-threaded, hence:
//    o Construction and destruction must take place from the same thread.
//    o All APIs must be called from the creating thread as well.
// - It is required to first acquire the native audio parameters of the default
//   output device and then use the same rate when creating this object.
//   Open() will fail unless "perfect" audio parameters are utilized.
// - Calling Close() also leads to self destruction.
// - Support for 8-bit audio has not yet been verified and tested.
//
// Core Audio API details:
//
// - The public API methods (Open(), Start(), Stop() and Close()) must be
//   called on constructing thread. The reason is that we want to ensure that
//   the COM environment is the same for all API implementations.
// - Utilized MMDevice interfaces:
//     o IMMDeviceEnumerator
//     o IMMDevice
// - Utilized WASAPI interfaces:
//     o IAudioClient
//     o IAudioRenderClient
// - The stream is initialized in shared mode and the processing of the
//   audio buffer is event driven.
// - The Multimedia Class Scheduler service (MMCSS) is utilized to boost
//   the priority of the render thread.
// - Audio-rendering endpoint devices can have three roles:
//   Console (eConsole), Communications (eCommunications), and Multimedia
//   (eMultimedia). Search for "Device Roles" on MSDN for more details.
//
// Threading details:
//
// - It is assumed that this class is created on the audio thread owned
//   by the AudioManager.
// - It is a requirement to call the following methods on the same audio
//   thread: Open(), Start(), Stop(), and Close().
// - Audio rendering is performed on the audio render thread, owned by this
//   class, and the AudioSourceCallback::OnMoreData() method will be called
//   from this thread. Stream switching also takes place on the audio-render
//   thread.
//
// Experimental exclusive mode:
//
// - It is possible to open up a stream in exclusive mode by using the
//   --enable-exclusive-audio command line flag.
// - The internal buffering scheme is less flexible for exclusive streams.
//   Hence, some manual tuning will be required before deciding what frame
//   size to use. See the WinAudioOutputTest unit test for more details.
// - If an application opens a stream in exclusive mode, the application has
//   exclusive use of the audio endpoint device that plays the stream.
// - Exclusive-mode should only be utilized when the lowest possible latency
//   is important.
// - In exclusive mode, the client can choose to open the stream in any audio
//   format that the endpoint device supports, i.e. not limited to the device's
//   current (default) configuration.
// - Initial measurements on Windows 7 (HP Z600 workstation) have shown that
//   the lowest possible latencies we can achieve on this machine are:
//     o ~3.3333ms @ 48kHz <=> 160 audio frames per buffer.
//     o ~3.6281ms @ 44.1kHz <=> 160 audio frames per buffer.
// - See
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844(v=vs.85).aspx
//   for more details.

#ifndef MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_

#include <Audioclient.h>
#include <MMDeviceAPI.h>
#include <audiopolicy.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/system_glitch_reporter.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioManagerWin;
class AudioSessionEventListener;
class AmplitudePeakDetector;

// AudioOutputStream implementation using Windows Core Audio APIs.
class MEDIA_EXPORT WASAPIAudioOutputStream
    : public AudioOutputStream,
      public base::DelegateSimpleThread::Delegate {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIAudioOutputStream(AudioManagerWin* manager,
                          const std::string& device_id,
                          const AudioParameters& params,
                          ERole device_role,
                          AudioManager::LogCallback log_callback);

  WASAPIAudioOutputStream(const WASAPIAudioOutputStream&) = delete;
  WASAPIAudioOutputStream& operator=(const WASAPIAudioOutputStream&) = delete;

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  ~WASAPIAudioOutputStream() override;

  // Implementation of AudioOutputStream.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Close() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

  // Returns AUDCLNT_SHAREMODE_EXCLUSIVE if --enable-exclusive-mode is used
  // as command-line flag and AUDCLNT_SHAREMODE_SHARED otherwise (default).
  static AUDCLNT_SHAREMODE GetShareMode();

  bool started() const { return render_thread_.get() != NULL; }

 private:
  PRINTF_FORMAT(2, 3) void SendLogMessage(const char* format, ...);

  // DelegateSimpleThread::Delegate implementation.
  void Run() override;

  // Core part of the thread loop which controls the actual rendering.
  // Checks available amount of space in the endpoint buffer and reads
  // data from the client to fill up the buffer without causing audio
  // glitches.
  bool RenderAudioFromSource(UINT64 device_frequency);

  // Called when the device will be opened in exclusive mode and use the
  // application specified format.
  // TODO(henrika): rewrite and move to CoreAudioUtil when removing flag
  // for exclusive audio mode.
  HRESULT ExclusiveModeInitialization(IAudioClient* client,
                                      HANDLE event_handle,
                                      uint32_t* endpoint_buffer_size);

  // If |render_thread_| is valid, sets |stop_render_event_| and blocks until
  // the thread has stopped.  |stop_render_event_| is reset after the call.
  // |source_| is set to NULL.
  void StopThread();

  // Reports audio stream glitch stats and resets them to their initial values.
  void ReportAndResetStats();

  // Start or stop listening for Windows audio session disconnected events. Uses
  // `audio_session_control_` to call
  // IAudioSessionControl::Register/UnregisterAudioSessionNotification() with
  // `session_listener_` as the event target.
  void StartAudioSessionEventListener();
  void StopAudioSessionEventListener();

  // Called by AudioSessionEventListener() when a device change occurs.
  void OnDeviceChanged();

  // Set up the desired render format specified by the client.
  void SetupWaveFormat();

  // Contains the thread ID of the creating thread.
  const base::PlatformThreadId creating_thread_id_;

  // Our creator, the audio manager needs to be notified when we close.
  const raw_ptr<AudioManagerWin> manager_;

  // Used to aggregate and report glitch metrics to UMA (periodically) and to
  // text logs (when a stream ends).
  SystemGlitchReporter glitch_reporter_;

  std::unique_ptr<AmplitudePeakDetector> peak_detector_;

  // Rendering is driven by this thread (which has no message loop).
  // All OnMoreData() callbacks will be called from this thread.
  std::unique_ptr<base::DelegateSimpleThread> render_thread_;

  // Contains the desired audio format which is set up at construction.
  // Extended PCM waveform format structure based on WAVEFORMATEXTENSIBLE.
  // Use this for multiple channel and hi-resolution PCM data.
  WAVEFORMATPCMEX format_;

  // AudioParameters from the constructor.
  const AudioParameters params_;

  // Set to true when stream is successfully opened.
  bool opened_;

  // Volume level from 0 to 1.
  float volume_;

  // Size in audio frames of each audio packet where an audio packet
  // is defined as the block of data which the source is expected to deliver
  // in each OnMoreData() callback.
  size_t packet_size_frames_;

  // If requesting an explicitly-lower frame size using the IAudioClient3
  // interface this is the requested size.
  size_t requested_iaudioclient3_buffer_size_;

  // Size in bytes of each audio packet.
  size_t packet_size_bytes_;

  // Length of the audio endpoint buffer.
  uint32_t endpoint_buffer_size_frames_;

  // The target device id or an empty string for the default device.
  const std::string device_id_;

  // Defines the role that the system has assigned to an audio endpoint device.
  const ERole device_role_;

  // The sharing mode for the connection.
  // Valid values are AUDCLNT_SHAREMODE_SHARED and AUDCLNT_SHAREMODE_EXCLUSIVE
  // where AUDCLNT_SHAREMODE_SHARED is the default.
  const AUDCLNT_SHAREMODE share_mode_;

  // Counts the number of audio frames written to the endpoint buffer.
  UINT64 num_written_frames_;

  // The position read during the last call to RenderAudioFromSource.
  UINT64 last_position_ = 0;

  // The performance counter read during the last call to RenderAudioFromSource.
  UINT64 last_qpc_position_ = 0;

  // Pointer to the client that will deliver audio samples to be played out.
  raw_ptr<AudioSourceCallback> source_;

  // Callback to send log messages to registered clients.
  AudioManager::LogCallback log_callback_;

  // An IAudioClient interface which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;

  // The IAudioRenderClient interface enables a client to write output
  // data to a rendering endpoint buffer.
  Microsoft::WRL::ComPtr<IAudioRenderClient> audio_render_client_;

  // The audio engine will signal this event each time a buffer becomes
  // ready to be filled by the client.
  base::win::ScopedHandle audio_samples_render_event_;

  // This event will be signaled when rendering shall stop.
  base::win::ScopedHandle stop_render_event_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  std::unique_ptr<AudioBus> audio_bus_;

  Microsoft::WRL::ComPtr<IAudioClock> audio_clock_;

  bool device_changed_ = false;

  bool enable_audio_offload_ = false;

  // Generates Windows audio session events for `session_listener_` to handle.
  Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;
  Microsoft::WRL::ComPtr<AudioSessionEventListener> session_listener_;

  // Since AudioSessionEventListener needs to posts tasks back to the audio
  // thread, it's possible to end up in a state where that task would execute
  // after destruction of this class -- so use a WeakPtr to cancel safely.
  base::WeakPtrFactory<WASAPIAudioOutputStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_
