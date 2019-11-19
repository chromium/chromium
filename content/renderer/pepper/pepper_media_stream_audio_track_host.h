// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/pepper/pepper_media_stream_track_host_base.h"
#include "media/base/audio_parameters.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/shared_impl/media_stream_audio_track_shared.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

class PepperMediaStreamAudioTrackHost : public PepperMediaStreamTrackHostBase {
 public:
  PepperMediaStreamAudioTrackHost(RendererPpapiHost* host,
                                  PP_Instance instance,
                                  PP_Resource resource,
                                  const blink::WebMediaStreamTrack& track);

 private:
  // A helper class for receiving audio samples in the audio thread.
  // This class is created and destroyed on the renderer main thread.
  class AudioSink : public blink::WebMediaStreamAudioSink {
   public:
    explicit AudioSink(PepperMediaStreamAudioTrackHost* host);
    ~AudioSink() override;

    // Enqueues a free buffer index into |buffers_| which will be used for
    // sending audio samples to plugin.
    // This function is called on the main thread.
    void EnqueueBuffer(int32_t index);

    // This function is called on the main thread.
    int32_t Configure(int32_t number_of_buffers, int32_t duration,
                      const ppapi::host::ReplyMessageContext& context);

    // Send a reply to the currently pending |Configure()| request.
    void SendConfigureReply(int32_t result);

    // blink::WebMediaStreamAudioSink overrides:
    // These two functions should be called on the audio thread.
    // NOTE: For this specific instance, |OnSetFormat()| is also called on the
    // main thread. However, the call to |OnSetFormat()| happens before this
    // sink is added to an audio track, also on the main thread, which should
    // avoid any potential races.
    void OnData(const media::AudioBus& audio_bus,
                base::TimeTicks estimated_capture_time) override;
    void OnSetFormat(const media::AudioParameters& params) override;

   private:
    // Initializes buffers on the main thread.
    void SetFormatOnMainThread(int bytes_per_second, int bytes_per_frame);

    void InitBuffers();

    // Send enqueue buffer message on the main thread.
    void SendEnqueueBufferMessageOnMainThread(int32_t index,
                                              int32_t buffers_generation);

    // Unowned host which is available during the AudioSink's lifespan.
    // It is mainly used in the main thread. But the audio thread will use
    // host_->buffer_manager() to read some buffer properties. It is safe
    // because the buffer_manager()'s properties will not be changed after
    // initialization.
    PepperMediaStreamAudioTrackHost* host_;

    // The estimated capture time of the first sample frame of audio. This is
    // used as the timebase to compute the buffer timestamps.
    // Access only on the audio thread.
    base::TimeTicks first_frame_capture_time_;

    // The current audio parameters.
    // Access only on the audio thread.
    media::AudioParameters audio_params_;

    // Index of the currently active buffer.
    // Access only on the audio thread.
    int active_buffer_index_;

    // Generation of buffers corresponding to the currently active
    // buffer. Used to make sure the active buffer is still valid.
    // Access only on the audio thread.
    int32_t active_buffers_generation_;

    // Current offset, in sample frames, within the currently active buffer.
    // Access only on the audio thread.
    int active_buffer_frame_offset_;

    // A lock to protect the index queue |buffers_|, |buffers_generation_|,
    // buffers in |host_->buffer_manager()|, and |output_buffer_size_|.
    base::Lock lock_;

    // A queue for free buffer indices.
    base::circular_deque<int32_t> buffers_;

    // Generation of buffers. It is increased by every |InitBuffers()| call.
    int32_t buffers_generation_;

    // Intended size of each output buffer.
    int32_t output_buffer_size_;

    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

    base::ThreadChecker audio_thread_checker_;

    // Number of buffers.
    int32_t number_of_buffers_;

    // Number of bytes per second.
    int bytes_per_second_;

    // Number of bytes per frame = channels * bytes per sample.
    int bytes_per_frame_;

    // User-configured buffer duration, in milliseconds.
    int32_t user_buffer_duration_;

    // Pending |Configure()| reply context.
    ppapi::host::ReplyMessageContext pending_configure_reply_;

    base::WeakPtrFactory<AudioSink> weak_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(AudioSink);
  };

  ~PepperMediaStreamAudioTrackHost() override;

  // ResourceMessageHandler overrides:
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // Message handlers:
  int32_t OnHostMsgConfigure(
      ppapi::host::HostMessageContext* context,
      const ppapi::MediaStreamAudioTrackShared::Attributes& attributes);

  // PepperMediaStreamTrackHostBase overrides:
  void OnClose() override;

  // MediaStreamBufferManager::Delegate overrides:
  void OnNewBufferEnqueued() override;

  // ResourceHost overrides:
  void DidConnectPendingHostToResource() override;

  blink::WebMediaStreamTrack track_;

  // True if |audio_sink_| has been added to |blink::WebMediaStreamTrack|
  // as a sink.
  bool connected_;

  AudioSink audio_sink_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaStreamAudioTrackHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_STREAM_AUDIO_TRACK_HOST_H_
