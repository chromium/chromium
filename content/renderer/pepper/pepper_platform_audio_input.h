// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_INPUT_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_INPUT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_input_ipc.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioParameters;
}

namespace content {

class PepperAudioInputHost;
class PepperMediaDeviceManager;

// PepperPlatformAudioInput is operated on two threads: the main thread (the
// thread on which objects are created) and the I/O thread. All public methods,
// except the destructor, must be called on the main thread. The notifications
// to the users of this class (i.e. PepperAudioInputHost) are also sent on the
// main thread. Internally, this class sends audio input IPC messages and
// receives media::AudioInputIPCDelegate notifications on the I/O thread.

class PepperPlatformAudioInput
    : public media::AudioInputIPCDelegate,
      public base::RefCountedThreadSafe<PepperPlatformAudioInput> {
 public:
  // Factory function, returns NULL on failure. StreamCreated() will be called
  // when the stream is created.
  static PepperPlatformAudioInput* Create(
      int render_frame_id,
      const std::string& device_id,
      int sample_rate,
      int frames_per_buffer,
      PepperAudioInputHost* client);

  PepperPlatformAudioInput(const PepperPlatformAudioInput&) = delete;
  PepperPlatformAudioInput& operator=(const PepperPlatformAudioInput&) = delete;

  // Called on main thread.
  void StartCapture();
  void StopCapture();
  // Closes the stream. Make sure to call this before the object is destructed.
  void ShutDown();

  // media::AudioInputIPCDelegate.
  void OnStreamCreated(base::ReadOnlySharedMemoryRegion shared_memory_region,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool initially_muted) override;
  void OnError(media::AudioCapturerSource::ErrorCode code) override;
  void OnMuted(bool is_muted) override;
  void OnIPCClosed() override;

 protected:
  ~PepperPlatformAudioInput() override;

 private:
  friend class base::RefCountedThreadSafe<PepperPlatformAudioInput>;

  PepperPlatformAudioInput();

  bool Initialize(int render_frame_id,
                  const std::string& device_id,
                  int sample_rate,
                  int frames_per_buffer,
                  PepperAudioInputHost* client);

  // I/O thread backends to above functions.
  void InitializeOnIOThread(const base::UnguessableToken& session_id);
  void StartCaptureOnIOThread();
  void StopCaptureOnIOThread();
  void ShutDownOnIOThread();

  void OnDeviceOpened(int request_id, bool succeeded, const std::string& label);
  void CloseDevice();
  void NotifyStreamCreationFailed();

  // Can return NULL if the RenderFrame referenced by |render_frame_id_| has
  // gone away.
  PepperMediaDeviceManager* GetMediaDeviceManager();

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  raw_ptr<PepperAudioInputHost> client_ = nullptr;

  // Used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O THREAD.
  std::unique_ptr<media::AudioInputIPC> ipc_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The frame containing the Pepper widget.
  int render_frame_id_ = MSG_ROUTING_NONE;
  blink::LocalFrameToken render_frame_token_;

  // The unique ID to identify the opened device. THIS MUST ONLY BE ACCESSED ON
  // THE MAIN THREAD.
  std::string label_;

  // Initialized on the main thread and accessed on the I/O thread afterwards.
  media::AudioParameters params_;

  // Whether we have tried to create an audio stream. THIS MUST ONLY BE ACCESSED
  // ON THE I/O THREAD.
  bool create_stream_sent_ = false;

  // Whether we have a pending request to open a device. We have to make sure
  // there isn't any pending request before this object goes away.
  // THIS MUST ONLY BE ACCESSED ON THE MAIN THREAD.
  bool pending_open_device_ = false;
  // THIS MUST ONLY BE ACCESSED ON THE MAIN THREAD.
  int pending_open_device_id_ = -1;

  // Used to handle cases where (Start|Stop)CaptureOnIOThread runs before the
  // InitializeOnIOThread. THIS MUST ONLY BE ACCESSED ON THE IO THREAD.
  enum { kIdle, kStarted, kStopped } ipc_startup_state_ = kIdle;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_INPUT_H_
