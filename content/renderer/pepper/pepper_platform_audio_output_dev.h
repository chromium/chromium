// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_DEV_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_DEV_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/audio_parameters.h"
#include "media/base/output_device_info.h"

namespace base {
class OneShotTimer;
class SingleThreadTaskRunner;
}

namespace content {
class PepperAudioOutputHost;

// This class is used to support new PPAPI |PPB_AudioOutput_Dev|, while
// |PepperPlatformAudioOutput| is to support old PPAPI |PPB_Audio|.
class PepperPlatformAudioOutputDev
    : public media::AudioOutputIPCDelegate,
      public base::RefCountedThreadSafe<PepperPlatformAudioOutputDev> {
 public:
  // Factory function, returns NULL on failure. StreamCreated() will be called
  // when the stream is created.
  static PepperPlatformAudioOutputDev* Create(int render_frame_id,
                                              const std::string& device_id,
                                              int sample_rate,
                                              int frames_per_buffer,
                                              PepperAudioOutputHost* client);

  // The following three methods are all called on main thread.

  // Request authorization to use the device.
  void RequestDeviceAuthorization();

  // Starts the playback. Returns false on error or if called before the
  // stream is created or after the stream is closed.
  bool StartPlayback();

  // Stops the playback. Returns false on error or if called before the stream
  // is created or after the stream is closed.
  bool StopPlayback();

  // Sets the volume. Returns false on error or if called before the stream
  // is created or after the stream is closed.
  bool SetVolume(double volume);

  // Closes the stream. Make sure to call this before the object is
  // destructed.
  void ShutDown();

  // media::AudioOutputIPCDelegate implementation.
  void OnError() override;
  void OnDeviceAuthorized(media::OutputDeviceStatus device_status,
                          const media::AudioParameters& output_params,
                          const std::string& matched_device_id) override;
  void OnStreamCreated(base::UnsafeSharedMemoryRegion shared_memory_region,
                       base::SyncSocket::Handle socket_handle,
                       bool playing_automatically) override;
  void OnIPCClosed() override;

 protected:
  ~PepperPlatformAudioOutputDev() override;

 private:
  enum State {
    IPC_CLOSED,       // No more IPCs can take place.
    IDLE,             // Not started.
    AUTHORIZING,      // Sent device authorization request, waiting for reply.
    AUTHORIZED,       // Successful device authorization received.
    CREATING_STREAM,  // Waiting for OnStreamCreated() to be called back.
    PAUSED,   // Paused.  OnStreamCreated() has been called.  Can Play()/Stop().
    PLAYING,  // Playing back.  Can Pause()/Stop().
  };

  friend class base::RefCountedThreadSafe<PepperPlatformAudioOutputDev>;

  PepperPlatformAudioOutputDev();
  PepperPlatformAudioOutputDev(int render_frame_id,
                               const std::string& device_id,
                               base::TimeDelta authorization_timeout);

  // Creates audio stream. Used for new Pepper audio output interface
  // |PPB_AudioOutput_Dev|.
  bool Initialize(int sample_rate,
                  int frames_per_buffer,
                  PepperAudioOutputHost* client);

  void RequestDeviceAuthorizationOnIOThread();
  void CreateStreamOnIOThread(const media::AudioParameters& params);
  void StartPlaybackOnIOThread();
  void StopPlaybackOnIOThread();
  void SetVolumeOnIOThread(double volume);
  void ShutDownOnIOThread();

  void NotifyStreamCreationFailed();

  PepperAudioOutputHost* client_;

  // Used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  std::unique_ptr<media::AudioOutputIPC> ipc_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The frame containing the Pepper widget.
  int render_frame_id_;

  // Initialized on the main thread and accessed on the I/O thread afterwards.
  media::AudioParameters params_;

  // Current state (must only be accessed from the IO thread).
  State state_;

  // State of Start() calls before OnDeviceAuthorized() is called.
  bool start_on_authorized_;

  // State of StartPlayback() calls before OnStreamCreated() is called.
  bool play_on_start_;

  // The media session ID used to identify which output device to be started.
  base::UnguessableToken session_id_;

  // ID of hardware output device to be used (provided session_id_ is zero)
  const std::string device_id_;

  // If |device_id_| is empty and |session_id_| is not, |matched_device_id_| is
  // received in OnDeviceAuthorized().
  std::string matched_device_id_;

  base::WaitableEvent did_receive_auth_;
  media::AudioParameters output_params_;
  media::OutputDeviceStatus device_status_;

  const base::TimeDelta auth_timeout_;
  std::unique_ptr<base::OneShotTimer> auth_timeout_action_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformAudioOutputDev);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_DEV_H_
