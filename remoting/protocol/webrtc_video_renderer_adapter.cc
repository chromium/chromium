// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_renderer_adapter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "remoting/codec/utils.h"
#include "remoting/protocol/client_video_stats_dispatcher.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace remoting::protocol {

namespace {

// Maximum number of ClientFrameStats instances to keep.
const int kMaxQueuedStats = 200;

base::span<const uint8_t> GetYuvPlaneSpan(const uint8_t* data,
                                          int stride,
                                          int width,
                                          int rows) {
  CHECK(data);
  CHECK_GT(width, 0);
  CHECK_GT(rows, 0);
  CHECK_GE(stride, width);
  // SAFETY: `CHECK`s above ensure `data` is non-null, `rows >= 1`, `width >=
  // 1`, and `stride >= width`, and `webrtc::PlanarYuv8Buffer` guarantees at
  // least `rows` rows of `width` bytes spaced `stride` bytes apart.
  return UNSAFE_BUFFERS(base::span(
      data,
      static_cast<size_t>(stride) * (rows - 1) + static_cast<size_t>(width)));
}

std::unique_ptr<webrtc::DesktopFrame> ConvertYuvToRgb(
    scoped_refptr<webrtc::VideoFrameBuffer> yuv_frame,
    std::unique_ptr<webrtc::DesktopFrame> rgb_frame,
    FrameConsumer::PixelFormat pixel_format) {
  DCHECK(rgb_frame->size().equals(
      webrtc::DesktopSize(yuv_frame->width(), yuv_frame->height())));
  CHECK_EQ(rgb_frame->pixel_format(), webrtc::FOURCC_ARGB);
  if (rgb_frame->size().is_empty()) {
    return rgb_frame;
  }

  webrtc::scoped_refptr<const webrtc::I420BufferInterface> i420_frame =
      yuv_frame->ToI420();
  if (!i420_frame) {
    LOG(ERROR) << "Failed to convert video frame to I420.";
    return nullptr;
  }
  CHECK_EQ(i420_frame->width(), rgb_frame->size().width());
  CHECK_EQ(i420_frame->height(), rgb_frame->size().height());

  const int width = i420_frame->width();
  const int height = i420_frame->height();
  const int uv_width = i420_frame->ChromaWidth();
  const int uv_height = i420_frame->ChromaHeight();
  const base::span<const uint8_t> y_span = GetYuvPlaneSpan(
      i420_frame->DataY(), i420_frame->StrideY(), width, height);
  const base::span<const uint8_t> u_span = GetYuvPlaneSpan(
      i420_frame->DataU(), i420_frame->StrideU(), uv_width, uv_height);
  const base::span<const uint8_t> v_span = GetYuvPlaneSpan(
      i420_frame->DataV(), i420_frame->StrideV(), uv_width, uv_height);

  const webrtc::FourCC output_fourcc =
      (pixel_format == FrameConsumer::FORMAT_BGRA) ? webrtc::FOURCC_ARGB
                                                   : webrtc::FOURCC_ABGR;
  ConvertYuvToDesktopFrame(y_span, i420_frame->StrideY(), u_span,
                           i420_frame->StrideU(), v_span, i420_frame->StrideV(),
                           /*is_i444=*/false, output_fourcc, *rgb_frame);

  rgb_frame->mutable_updated_region()->AddRect(
      webrtc::DesktopRect::MakeSize(rgb_frame->size()));
  return rgb_frame;
}

}  // namespace

WebrtcVideoRendererAdapter::WebrtcVideoRendererAdapter(
    const std::string& label,
    VideoRenderer* video_renderer)
    : label_(label),
      video_renderer_(video_renderer),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      convert_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

WebrtcVideoRendererAdapter::~WebrtcVideoRendererAdapter() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Needed for ConnectionTest unittests which set up a fake connection without
  // starting any video. This video adapter is instantiated when the incoming
  // video-stats data channel is created.
  if (video_track_) {
    video_track_->RemoveSink(this);
  }
}

void WebrtcVideoRendererAdapter::SetMediaStream(
    webrtc::scoped_refptr<webrtc::MediaStreamInterface> media_stream) {
  DCHECK_EQ(media_stream->id(), label());

  if (video_track_) {
    video_track_->RemoveSink(this);
  }

  webrtc::VideoTrackVector video_tracks = media_stream->GetVideoTracks();

  // Caller must verify that the media stream contains video tracks.
  CHECK(!video_tracks.empty());

  if (video_tracks.size() > 1U) {
    LOG(WARNING) << "Received media stream with multiple video tracks.";
  }

  video_track_ = video_tracks[0];
  video_track_->AddOrUpdateSink(this, webrtc::VideoSinkWants());
}

void WebrtcVideoRendererAdapter::SetVideoStatsChannel(
    std::unique_ptr<MessagePipe> message_pipe) {
  // Expect that the host also creates video_stats data channel.
  video_stats_dispatcher_ =
      std::make_unique<ClientVideoStatsDispatcher>(label_, this);
  video_stats_dispatcher_->Init(std::move(message_pipe), this);
}

void WebrtcVideoRendererAdapter::OnFrame(const webrtc::VideoFrame& frame) {
  if (frame.timestamp_us() > webrtc::TimeMicros()) {
    // The host sets playout delay to 0, so all incoming frames are expected to
    // be rendered as so as they are received.
    NOTREACHED() << "Received frame with playout delay greater than 0.";
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoRendererAdapter::HandleFrameOnMainThread,
                     weak_factory_.GetWeakPtr(), frame.rtp_timestamp(),
                     base::TimeTicks::Now(),
                     scoped_refptr<webrtc::VideoFrameBuffer>(
                         frame.video_frame_buffer().get())));
}

void WebrtcVideoRendererAdapter::OnVideoFrameStats(
    uint32_t rtp_timestamp,
    const HostFrameStats& host_stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Drop all ClientFrameStats for frames before |rtp_timestamp|. Stats messages
  // are expected to be received in the same order as the corresponding video
  // frames, so we are not going to receive HostFrameStats for the frames before
  // |rtp_timestamp|. This may happen only if for some reason the host doesn't
  // generate stats message for all video frames.
  while (!client_stats_queue_.empty() &&
         client_stats_queue_.front().first != rtp_timestamp) {
    client_stats_queue_.pop_front();
  }

  // If there are no ClientFrameStats in the queue then queue HostFrameStats
  // to be processed in FrameRendered().
  if (client_stats_queue_.empty()) {
    if (host_stats_queue_.size() > kMaxQueuedStats) {
      LOG(ERROR) << "video_stats channel is out of sync with the video stream. "
                    "Performance stats will not be reported.";
      video_stats_dispatcher_.reset();
      return;
    }
    host_stats_queue_.emplace_back(rtp_timestamp, host_stats);
    return;
  }

  // The correspond frame has been received and now we have both HostFrameStats
  // and ClientFrameStats. Report the stats to FrameStatsConsumer.
  DCHECK_EQ(client_stats_queue_.front().first, rtp_timestamp);
  FrameStats frame_stats;
  frame_stats.client_stats = client_stats_queue_.front().second;
  client_stats_queue_.pop_front();
  frame_stats.host_stats = host_stats;
  FrameStatsConsumer* frame_stats_consumer =
      video_renderer_->GetFrameStatsConsumer();
  if (frame_stats_consumer) {
    frame_stats_consumer->OnVideoFrameStats(frame_stats);
  }
}

void WebrtcVideoRendererAdapter::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {}

void WebrtcVideoRendererAdapter::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  LOG(WARNING) << "video_stats channel was closed by the host.";
}

void WebrtcVideoRendererAdapter::HandleFrameOnMainThread(
    uint32_t rtp_timestamp,
    base::TimeTicks time_received,
    scoped_refptr<webrtc::VideoFrameBuffer> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::unique_ptr<ClientFrameStats> stats(new ClientFrameStats());
  // TODO(sergeyu): |time_received| is not reported correctly here because the
  // frame is already decoded at this point.
  stats->time_received = time_received;

  std::unique_ptr<webrtc::DesktopFrame> rgb_frame =
      video_renderer_->GetFrameConsumer()->AllocateFrame(
          webrtc::DesktopSize(frame->width(), frame->height()));

  convert_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ConvertYuvToRgb, std::move(frame), std::move(rgb_frame),
                     video_renderer_->GetFrameConsumer()->GetPixelFormat()),
      base::BindOnce(&WebrtcVideoRendererAdapter::DrawFrame,
                     weak_factory_.GetWeakPtr(), rtp_timestamp,
                     std::move(stats)));
}

void WebrtcVideoRendererAdapter::DrawFrame(
    uint32_t rtp_timestamp,
    std::unique_ptr<ClientFrameStats> stats,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!frame) {
    return;
  }
  stats->time_decoded = base::TimeTicks::Now();
  video_renderer_->GetFrameConsumer()->DrawFrame(
      std::move(frame),
      base::BindOnce(&WebrtcVideoRendererAdapter::FrameRendered,
                     weak_factory_.GetWeakPtr(), rtp_timestamp,
                     std::move(stats)));
}

void WebrtcVideoRendererAdapter::FrameRendered(
    uint32_t rtp_timestamp,
    std::unique_ptr<ClientFrameStats> client_stats) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!video_stats_dispatcher_ || !video_stats_dispatcher_->is_connected()) {
    return;
  }

  client_stats->time_rendered = base::TimeTicks::Now();

  // Drop all HostFrameStats for frames before |rtp_timestamp|. Stats messages
  // are expected to be received in the same order as the corresponding video
  // frames. This may happen only if the host generates HostFrameStats without
  // the corresponding frame.
  while (!host_stats_queue_.empty() &&
         host_stats_queue_.front().first != rtp_timestamp) {
    LOG(WARNING) << "Host sent VideoStats message for a frame that was never "
                    "received.";
    host_stats_queue_.pop_front();
  }

  // If HostFrameStats hasn't been received for |rtp_timestamp| then queue
  // ClientFrameStats to be processed in OnVideoFrameStats().
  if (host_stats_queue_.empty()) {
    if (client_stats_queue_.size() > kMaxQueuedStats) {
      LOG(ERROR) << "video_stats channel is out of sync with the video "
                    "stream. Performance stats will not be reported.";
      video_stats_dispatcher_.reset();
      return;
    }
    client_stats_queue_.emplace_back(rtp_timestamp, *client_stats);
    return;
  }

  // The correspond HostFrameStats has been received already and now we have
  // both HostFrameStats and ClientFrameStats. Report the stats to
  // FrameStatsConsumer.
  DCHECK_EQ(host_stats_queue_.front().first, rtp_timestamp);
  FrameStats frame_stats;
  frame_stats.host_stats = host_stats_queue_.front().second;
  frame_stats.client_stats = *client_stats;
  host_stats_queue_.pop_front();
  FrameStatsConsumer* frame_stats_consumer =
      video_renderer_->GetFrameStatsConsumer();
  if (frame_stats_consumer) {
    frame_stats_consumer->OnVideoFrameStats(frame_stats);
  }
}

}  // namespace remoting::protocol
