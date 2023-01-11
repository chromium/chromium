// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the main interface for the cast receiver. All configuration are done
// at creation.

#ifndef MEDIA_CAST_TEST_RECEIVER_CAST_RECEIVER_H_
#define MEDIA_CAST_TEST_RECEIVER_CAST_RECEIVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport.h"

namespace media {
class VideoFrame;

namespace cast {

// The following callbacks are used to deliver decoded audio/video frame data,
// the frame's corresponding play-out time, and a continuity flag.
// |is_continuous| will be false to indicate the loss of data due to a loss of
// frames (or decoding errors).  This allows the client to take steps to smooth
// discontinuities for playback.  Note: A NULL pointer can be returned when data
// is not available (e.g., bad/missing packet).
using AudioFrameDecodedCallback =
    base::RepeatingCallback<void(std::unique_ptr<AudioBus> audio_bus,
                                 base::TimeTicks playout_time,
                                 bool is_continuous)>;
using VideoFrameDecodedCallback =
    base::RepeatingCallback<void(scoped_refptr<media::VideoFrame> video_frame,
                                 base::TimeTicks playout_time,
                                 bool is_continuous)>;

class CastReceiver {
 public:
  static std::unique_ptr<CastReceiver> Create(
      scoped_refptr<CastEnvironment> cast_environment,
      const FrameReceiverConfig& audio_config,
      const FrameReceiverConfig& video_config,
      CastTransport* const transport);

  // All received RTP and RTCP packets for the call should be sent to this
  // PacketReceiver.  Can be called from any thread.
  virtual void ReceivePacket(std::unique_ptr<Packet> packet) = 0;

  // Polling interface to get audio and video frames from the CastReceiver. The
  // the RequestDecodedXXXXXFrame() methods utilize internal software-based
  // decoding.
  //
  // In all cases, the given |callback| is guaranteed to be run at some point in
  // the future, except for those requests still enqueued at destruction time.
  //
  // These methods should all be called on the CastEnvironment's MAIN thread.
  virtual void RequestDecodedAudioFrame(
      const AudioFrameDecodedCallback& callback) = 0;
  virtual void RequestDecodedVideoFrame(
      const VideoFrameDecodedCallback& callback) = 0;

  virtual ~CastReceiver() {}
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_RECEIVER_CAST_RECEIVER_H_
