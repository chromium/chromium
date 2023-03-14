// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//  audio::OutputController              AudioOutputDevice
//           ^                                  ^
//           |                                  |
//           v                 IPC              v
//  audio::OutputStream  <---------> AudioOutputIPC (MojoAudioOutputIPC)
//
// Transportation of audio samples from the render to the browser process
// is done by using shared memory in combination with a sync socket pair
// to generate a low latency transport. The AudioOutputDevice user registers an
// AudioOutputDevice::RenderCallback at construction and will be polled by the
// AudioOutputController for audio to be played out by the underlying audio
// layers.
//
// State sequences.
//
//            Task [IO thread]                  IPC [IO thread]
// RequestDeviceAuthorization -> RequestDeviceAuthorizationOnIOThread ------>
// RequestDeviceAuthorization ->
//             <- OnDeviceAuthorized <- AudioMsg_NotifyDeviceAuthorized <-
//
// Start -> CreateStreamOnIOThread -----> CreateStream ------>
//       <- OnStreamCreated <- AudioMsg_NotifyStreamCreated <-
//       ---> PlayOnIOThread -----------> PlayStream -------->
//
// Optionally Play() / Pause() sequences may occur:
// Play -> PlayOnIOThread --------------> PlayStream --------->
// Pause -> PauseOnIOThread ------------> PauseStream -------->
// (note that Play() / Pause() sequences before
// OnStreamCreated are deferred until OnStreamCreated, with the last valid
// state being used)
//
// AudioOutputDevice::Render => audio transport on audio thread =>
//                               |
// Stop --> ShutDownOnIOThread -------->  CloseStream -> Close
//
// This class utilizes several threads during its lifetime, namely:
// 1. Creating thread.
//    Must be the main render thread.
// 2. Control thread (may be the main render thread or another thread).
//    The methods: Start(), Stop(), Play(), Pause(), SetVolume()
//    must be called on the same thread.
// 3. IO thread (internal implementation detail - not exposed to public API)
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 4. Audio transport thread (See AudioDeviceThread).
//    Responsible for calling the AudioOutputDeviceThreadCallback
//    implementation that in turn calls AudioRendererSink::RenderCallback
//    which feeds audio samples to the audio layer in the browser process using
//    sync sockets and shared memory.
//
// Implementation notes:
// - The user must call Stop() before deleting the class instance.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/audio/audio_device_thread.h"
#include "media/audio/audio_output_ipc.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_export.h"
#include "media/base/output_device_info.h"

namespace base {
class OneShotTimer;
class SingleThreadTaskRunner;
}

namespace media {
class AudioOutputDeviceThreadCallback;

class MEDIA_EXPORT AudioOutputDevice : public AudioRendererSink,
                                       public AudioOutputIPCDelegate {
 public:
  // NOTE: Clients must call Initialize() before using.
  AudioOutputDevice(
      std::unique_ptr<AudioOutputIPC> ipc,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const AudioSinkParameters& sink_params,
      base::TimeDelta authorization_timeout);

  AudioOutputDevice(const AudioOutputDevice&) = delete;
  AudioOutputDevice& operator=(const AudioOutputDevice&) = delete;

  // Request authorization to use the device specified in the constructor.
  void RequestDeviceAuthorization();

  // AudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  void Flush() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;

  // Methods called on IO thread ----------------------------------------------
  // AudioOutputIPCDelegate methods.
  void OnError() override;
  void OnDeviceAuthorized(OutputDeviceStatus device_status,
                          const AudioParameters& output_params,
                          const std::string& matched_device_id) override;
  void OnStreamCreated(base::UnsafeSharedMemoryRegion shared_memory_region,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool play_automatically) override;
  void OnIPCClosed() override;

  AudioOutputIPC* GetIpcForTesting() { return ipc_.get(); }

 protected:
  // Magic required by ref_counted.h to avoid any code deleting the object
  // accidentally while there are references to it.
  friend class base::RefCountedThreadSafe<AudioOutputDevice>;
  ~AudioOutputDevice() override;

 private:
  enum StartupState {
    IDLE,                       // Authorization not requested.
    AUTHORIZATION_REQUESTED,    // Sent (possibly completed) device
                                // authorization request.
    STREAM_CREATION_REQUESTED,  // Sent (possibly completed) device creation
                                // request. Can Play()/Pause()/Stop().
  };

  // This enum is used for UMA, so the only allowed operation on this definition
  // is to add new states to the bottom, update kMaxValue, and update the
  // histogram "Media.Audio.Render.StreamCallbackError2".
  enum Error {
    kNoError = 0,
    kErrorDuringCreation = 1,
    kErrorDuringRendering = 2,
    kMaxValue = kErrorDuringRendering
  };

  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that need to
  // be executed on that thread.  They use AudioOutputIPC to send IPC messages
  // upon state changes.
  void RequestDeviceAuthorizationOnIOThread();
  void InitializeOnIOThread(const AudioParameters& params,
                            MayBeDangling<RenderCallback> callback);
  void CreateStreamOnIOThread();
  void PlayOnIOThread();
  void PauseOnIOThread();
  void FlushOnIOThread();
  void ShutDownOnIOThread();
  void SetVolumeOnIOThread(double volume);

  // Process device authorization result on the IO thread.
  void ProcessDeviceAuthorizationOnIOThread(
      OutputDeviceStatus device_status,
      const AudioParameters& output_params,
      const std::string& matched_device_id,
      bool timed_out);

  void NotifyRenderCallbackOfError();

  OutputDeviceInfo GetOutputDeviceInfo_Signaled();
  void OnAuthSignal();

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  AudioParameters audio_parameters_;

  raw_ptr<RenderCallback, DanglingUntriaged> callback_;

  // A pointer to the IPC layer that takes care of sending requests over to
  // the implementation. May be set to nullptr after errors.
  std::unique_ptr<AudioOutputIPC> ipc_;

  // Current state (must only be accessed from the IO thread).  See comments for
  // State enum above.
  StartupState state_;

  // For UMA stats. May only be accessed on the IO thread.
  Error had_error_ = kNoError;

  // Last set volume.
  double volume_ = 1.0;

  // The media session ID used to identify which input device to be started.
  // Only used by Unified IO.
  base::UnguessableToken session_id_;

  // ID of hardware output device to be used (provided |session_id_| is zero)
  const std::string device_id_;

  // If |device_id_| is empty and |session_id_| is not, |matched_device_id_| is
  // received in OnDeviceAuthorized().
  std::string matched_device_id_;

  // In order to avoid a race between OnStreamCreated and Stop(), we use this
  // guard to control stopping and starting the audio thread.
  base::Lock audio_thread_lock_;
  std::unique_ptr<AudioOutputDeviceThreadCallback> audio_callback_;
  std::unique_ptr<AudioDeviceThread> audio_thread_
      GUARDED_BY(audio_thread_lock_);

  // Temporary hack to ignore OnStreamCreated() due to the user calling Stop()
  // so we don't start the audio thread pointing to a potentially freed
  // |callback_|.
  //
  // TODO(scherkus): Replace this by changing AudioRendererSink to either accept
  // the callback via Start(). See http://crbug.com/151051 for details.
  bool stopping_hack_ GUARDED_BY(audio_thread_lock_);

  base::WaitableEvent did_receive_auth_;
  AudioParameters output_params_;
  OutputDeviceStatus device_status_;

  const base::TimeDelta auth_timeout_;
  std::unique_ptr<base::OneShotTimer> auth_timeout_action_;

  // Pending callback for OutputDeviceInfo if it has not been received by the
  // time a call to GetGetOutputDeviceInfoAsync() is called.
  //
  // Lock for use ONLY with |pending_device_info_cb_| and |did_receive_auth_|,
  // if you add more usage of this lock ensure you have not added a deadlock.
  base::Lock device_info_lock_;
  OutputDeviceInfoCB pending_device_info_cb_ GUARDED_BY(device_info_lock_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_
