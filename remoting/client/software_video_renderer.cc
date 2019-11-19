// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/software_video_renderer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "remoting/base/util.h"
#include "remoting/client/client_context.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "remoting/codec/video_decoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/protocol/session_config.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using remoting::protocol::ChannelConfig;
using remoting::protocol::SessionConfig;

namespace remoting {

namespace {

std::unique_ptr<webrtc::DesktopFrame> DoDecodeFrame(
    VideoDecoder* decoder,
    std::unique_ptr<VideoPacket> packet,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (!decoder->DecodePacket(*packet, frame.get()))
    frame.reset();
  return frame;
}

}  // namespace

SoftwareVideoRenderer::SoftwareVideoRenderer(protocol::FrameConsumer* consumer)
    : consumer_(consumer) {
  thread_checker_.DetachFromThread();
}

SoftwareVideoRenderer::SoftwareVideoRenderer(
    std::unique_ptr<protocol::FrameConsumer> consumer)
    : SoftwareVideoRenderer(consumer.get()) {
  owned_consumer_ = std::move(consumer);
}

SoftwareVideoRenderer::~SoftwareVideoRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_)
    decode_task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
}

bool SoftwareVideoRenderer::Initialize(
    const ClientContext& client_context,
    protocol::FrameStatsConsumer* stats_consumer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  decode_task_runner_ = client_context.decode_task_runner();
  stats_consumer_ = stats_consumer;
  return true;
}

void SoftwareVideoRenderer::OnSessionConfig(
    const protocol::SessionConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Initialize decoder based on the selected codec.
  ChannelConfig::Codec codec = config.video_config().codec;
  if (codec == ChannelConfig::CODEC_VERBATIM) {
    decoder_.reset(new VideoDecoderVerbatim());
  } else if (codec == ChannelConfig::CODEC_VP8) {
    decoder_ = VideoDecoderVpx::CreateForVP8();
  } else if (codec == ChannelConfig::CODEC_VP9) {
    decoder_ = VideoDecoderVpx::CreateForVP9();
  } else if (codec == ChannelConfig::CODEC_H264) {
    NOTIMPLEMENTED() << "H264 software decoding is not supported.";
  } else {
    NOTREACHED() << "Invalid Encoding found: " << codec;
  }

  decoder_->SetPixelFormat(
      (consumer_->GetPixelFormat() == protocol::FrameConsumer::FORMAT_BGRA)
          ? VideoDecoder::PixelFormat::BGRA
          : VideoDecoder::PixelFormat::RGBA);
}

protocol::VideoStub* SoftwareVideoRenderer::GetVideoStub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return this;
}

protocol::FrameConsumer* SoftwareVideoRenderer::GetFrameConsumer() {
  return consumer_;
}

protocol::FrameStatsConsumer* SoftwareVideoRenderer::GetFrameStatsConsumer() {
  return stats_consumer_;
}

void SoftwareVideoRenderer::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> packet,
    base::OnceClosure done) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::ScopedClosureRunner done_runner(std::move(done));

  std::unique_ptr<protocol::FrameStats> frame_stats(new protocol::FrameStats());
  frame_stats->host_stats =
      protocol::HostFrameStats::GetForVideoPacket(*packet);
  frame_stats->client_stats.time_received = base::TimeTicks::Now();

  // If the video packet is empty then there is nothing to decode. Empty packets
  // are used to maintain activity on the network. Stats for such packets still
  // need to be reported.
  if (!packet->has_data() || packet->data().size() == 0) {
    if (stats_consumer_)
      stats_consumer_->OnVideoFrameStats(*frame_stats);
    return;
  }

  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    source_size_.set(packet->format().screen_width(),
                     packet->format().screen_height());
  }

  if (packet->format().has_x_dpi() && packet->format().has_y_dpi()) {
    webrtc::DesktopVector source_dpi(packet->format().x_dpi(),
                                     packet->format().y_dpi());
    if (!source_dpi.equals(source_dpi_)) {
      source_dpi_ = source_dpi;
    }
  }

  if (source_size_.is_empty()) {
    LOG(ERROR) << "Received VideoPacket with unknown size.";
    return;
  }

  std::unique_ptr<webrtc::DesktopFrame> frame =
      consumer_->AllocateFrame(source_size_);
  frame->set_dpi(source_dpi_);

  base::PostTaskAndReplyWithResult(
      decode_task_runner_.get(), FROM_HERE,
      base::Bind(&DoDecodeFrame, decoder_.get(), base::Passed(&packet),
                 base::Passed(&frame)),
      base::Bind(&SoftwareVideoRenderer::RenderFrame,
                 weak_factory_.GetWeakPtr(), base::Passed(&frame_stats),
                 base::AdaptCallbackForRepeating(done_runner.Release())));
}

void SoftwareVideoRenderer::RenderFrame(
    std::unique_ptr<protocol::FrameStats> stats,
    const base::Closure& done,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  stats->client_stats.time_decoded = base::TimeTicks::Now();
  if (!frame) {
    if (!done.is_null())
      done.Run();
    return;
  }

  consumer_->DrawFrame(
      std::move(frame),
      base::Bind(&SoftwareVideoRenderer::OnFrameRendered,
                 weak_factory_.GetWeakPtr(), base::Passed(&stats), done));
}

void SoftwareVideoRenderer::OnFrameRendered(
    std::unique_ptr<protocol::FrameStats> stats,
    const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());

  stats->client_stats.time_rendered = base::TimeTicks::Now();
  if (stats_consumer_)
    stats_consumer_->OnVideoFrameStats(*stats);

  if (!done.is_null())
    done.Run();
}

}  // namespace remoting
