// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_RENDERER_H_
#define REMOTING_PROTOCOL_VIDEO_RENDERER_H_

namespace remoting {

class ClientContext;

namespace protocol {

class FrameConsumer;
class FrameStatsConsumer;
class SessionConfig;
class VideoStub;

// VideoRenderer is responsible for decoding and displaying incoming video
// stream. This interface is used by ConnectionToHost implementations to
// render received video frames. ConnectionToHost may feed encoded frames to the
// VideoStub or decode them and pass decoded frames to the FrameConsumer. If the
// implementation uses FrameConsumer directly then it must also call
// FrameStatsConsumer with FrameStats for each frame.
//
// TODO(sergeyu): Reconsider this design.
class VideoRenderer {
 public:
  virtual ~VideoRenderer() = default;

  // Initializes the video renderer. This allows the renderer to be initialized
  // after it is constructed. Returns true if initialization succeeds and false
  // otherwise. An implementation that doesn't use this function to initialize
  // should always return true.
  // |perf_tracker| must outlive the renderer.
  virtual bool Initialize(const ClientContext& client_context,
                          protocol::FrameStatsConsumer* stats_consumer) = 0;

  // Configures the renderer with the supplied |config|. This must be called
  // exactly once before video data is supplied to the renderer.
  virtual void OnSessionConfig(const SessionConfig& config) = 0;

  // Returns the VideoStub interface of this renderer.
  virtual VideoStub* GetVideoStub() = 0;

  // Returns the FrameConsumer interface for this renderer.
  virtual FrameConsumer* GetFrameConsumer() = 0;

  // Returns the FrameStatsConsumer interface for this renderer.
  virtual FrameStatsConsumer* GetFrameStatsConsumer() = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_RENDERER_H_
