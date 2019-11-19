// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_CAST_CAST_SENDER_IMPL_H_
#define MEDIA_CAST_CAST_SENDER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"

namespace media {

namespace cast {
class AudioSender;
class VideoSender;

// This class combines all required sending objects such as the audio and video
// senders, pacer, packet receiver and frame input.
class CastSenderImpl : public CastSender {
 public:
  CastSenderImpl(scoped_refptr<CastEnvironment> cast_environment,
                 CastTransport* const transport_sender);

  void InitializeAudio(const FrameSenderConfig& audio_config,
                       const StatusChangeCallback& status_change_cb) final;
  void InitializeVideo(
      const FrameSenderConfig& video_config,
      const StatusChangeCallback& status_change_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb) final;

  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay) final;

  ~CastSenderImpl() final;

  scoped_refptr<AudioFrameInput> audio_frame_input() final;
  scoped_refptr<VideoFrameInput> video_frame_input() final;

 private:
  void ReceivedPacket(std::unique_ptr<Packet> packet);
  void OnAudioStatusChange(const StatusChangeCallback& status_change_cb,
                           OperationalStatus status);
  void OnVideoStatusChange(const StatusChangeCallback& status_change_cb,
                           OperationalStatus status);

  std::unique_ptr<AudioSender> audio_sender_;
  std::unique_ptr<VideoSender> video_sender_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;
  scoped_refptr<CastEnvironment> cast_environment_;
  // The transport sender is owned by the owner of the CastSender, and should be
  // valid throughout the lifetime of the CastSender.
  CastTransport* const transport_sender_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CastSenderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastSenderImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_IMPL_H_
