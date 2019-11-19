// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/client_video_stats_dispatcher.h"
#include "remoting/protocol/video_stats_stub.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/video/video_sink_interface.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
class VideoFrameBuffer;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class MessagePipe;
class VideoRenderer;
struct ClientFrameStats;
struct HostFrameStats;

class WebrtcVideoRendererAdapter
    : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public VideoStatsStub,
      public ClientVideoStatsDispatcher::EventHandler {
 public:
  WebrtcVideoRendererAdapter(const std::string& label,
                             VideoRenderer* video_renderer);
  ~WebrtcVideoRendererAdapter() override;

  std::string label() const { return label_; }

  void SetMediaStream(scoped_refptr<webrtc::MediaStreamInterface> media_stream);
  void SetVideoStatsChannel(std::unique_ptr<MessagePipe> message_pipe);

  // rtc::VideoSinkInterface implementation.
  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  // VideoStatsStub interface.
  void OnVideoFrameStats(uint32_t frame_id,
                         const HostFrameStats& frame_stats) override;

  // ClientVideoStatsDispatcher::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override;

  void HandleFrameOnMainThread(uint32_t frame_id,
                               base::TimeTicks time_received,
                               scoped_refptr<webrtc::VideoFrameBuffer> frame);
  void DrawFrame(uint32_t frame_id,
                 std::unique_ptr<ClientFrameStats> stats,
                 std::unique_ptr<webrtc::DesktopFrame> frame);
  void FrameRendered(uint32_t frame_id,
                     std::unique_ptr<ClientFrameStats> stats);

  std::string label_;

  scoped_refptr<webrtc::MediaStreamInterface> media_stream_;
  VideoRenderer* video_renderer_;

  std::unique_ptr<ClientVideoStatsDispatcher> video_stats_dispatcher_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::list<std::pair<uint32_t, ClientFrameStats>> client_stats_queue_;
  std::list<std::pair<uint32_t, HostFrameStats>> host_stats_queue_;

  base::WeakPtrFactory<WebrtcVideoRendererAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoRendererAdapter);
};

}  // namespace remoting
}  // namespace protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_
