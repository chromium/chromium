// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the main interface for the cast sender.
//
// The AudioFrameInput, VideoFrameInput and PacketReciever interfaces should
// be accessed from the main thread.

#ifndef MEDIA_CAST_CAST_SENDER_H_
#define MEDIA_CAST_CAST_SENDER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"
#include "media/cast/net/cast_transport.h"

namespace gfx {
class Size;
}

namespace media {

namespace cast {

class VideoFrameInput : public base::RefCountedThreadSafe<VideoFrameInput> {
 public:
  // Insert video frames into Cast sender. Frames will be encoded, packetized
  // and sent to the network.
  virtual void InsertRawVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                                   base::TimeTicks capture_time) = 0;

  // Creates a |VideoFrame| optimized for the encoder. When available, these
  // frames offer performance benefits, such as memory copy elimination. The
  // format is guaranteed to be I420 or NV12.
  //
  // Not every encoder supports this method. Use |CanCreateOptimizedFrames| to
  // determine if you can and should use this method.
  //
  // Even if |CanCreateOptimizedFrames| indicates support, there are transient
  // conditions during a session where optimized frames cannot be provided.  In
  // this case, the caller must be able to account for a nullptr return value
  // and instantiate its own media::VideoFrames.
  virtual scoped_refptr<VideoFrame> MaybeCreateOptimizedFrame(
      const gfx::Size& frame_size, base::TimeDelta timestamp) = 0;

  // Returns true if the encoder supports creating optimized frames.
  virtual bool CanCreateOptimizedFrames() const = 0;

 protected:
  virtual ~VideoFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<VideoFrameInput>;
};

class AudioFrameInput : public base::RefCountedThreadSafe<AudioFrameInput> {
 public:
  // Insert audio frames into Cast sender. Frames will be encoded, packetized
  // and sent to the network.
  virtual void InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                           const base::TimeTicks& recorded_time) = 0;

 protected:
  virtual ~AudioFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<AudioFrameInput>;
};

// Callback that is run to update the client with current status.  This is used
// to allow the client to wait for asynchronous initialization to complete
// before sending frames, and also to be notified of any runtime errors that
// have halted the session.
using StatusChangeCallback = base::Callback<void(OperationalStatus)>;

// All methods of CastSender must be called on the main thread.
// Provided CastTransport will also be called on the main thread.
class CastSender {
 public:
  static std::unique_ptr<CastSender> Create(
      scoped_refptr<CastEnvironment> cast_environment,
      CastTransport* const transport_sender);

  virtual ~CastSender() {}

  // All video frames for the session should be inserted to this object.
  virtual scoped_refptr<VideoFrameInput> video_frame_input() = 0;

  // All audio frames for the session should be inserted to this object.
  virtual scoped_refptr<AudioFrameInput> audio_frame_input() = 0;

  // Initialize the audio stack. Must be called in order to send audio frames.
  // |status_change_cb| will be run as operational status changes.
  virtual void InitializeAudio(
      const FrameSenderConfig& audio_config,
      const StatusChangeCallback& status_change_cb) = 0;

  // Initialize the video stack. Must be called in order to send video frames.
  // |status_change_cb| will be run as operational status changes.
  //
  // TODO(miu): Remove the VEA-specific callbacks.  http://crbug.com/454029
  virtual void InitializeVideo(
      const FrameSenderConfig& video_config,
      const StatusChangeCallback& status_change_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb) = 0;

  // Change the target delay. This is only valid if the receiver
  // supports the "adaptive_target_delay" rtp extension.
  virtual void SetTargetPlayoutDelay(
      base::TimeDelta new_target_playout_delay) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_H_
