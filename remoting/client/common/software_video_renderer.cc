// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/common/software_video_renderer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/common/logging.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "remoting/codec/video_decoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/session_config.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using remoting::protocol::ChannelConfig;
using remoting::protocol::SessionConfig;

namespace remoting {

namespace {

std::unique_ptr<webrtc::DesktopFrame> DoDecodeFrame(
    VideoDecoder* decoder,
    std::unique_ptr<VideoPacket> packet,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (!decoder->DecodePacket(*packet, frame.get())) {
    frame.reset();
  }
  return frame;
}

}  // namespace

SoftwareVideoRenderer::SoftwareVideoRenderer(protocol::FrameConsumer* consumer)
    : consumer_(consumer) {
  CHECK(consumer_);
  DETACH_FROM_THREAD(thread_checker_);
  decode_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
      base::TaskTraits{base::TaskPriority::USER_VISIBLE},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
}

SoftwareVideoRenderer::~SoftwareVideoRenderer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (decoder_) {
    decode_task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
  }
}

bool SoftwareVideoRenderer::Initialize(
    const ClientContext& client_context,
    protocol::FrameStatsConsumer* stats_consumer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return true;
}

void SoftwareVideoRenderer::OnSessionConfig(
    const protocol::SessionConfig& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Initialize decoder based on the selected codec.
  // TODO: joedow - Update this to support AV1 and use WebRTC decoders when we
  // the native code is updated to support WebRTC.
  ChannelConfig::Codec codec = config.video_config().codec;
  if (codec == ChannelConfig::CODEC_VERBATIM) {
    decoder_ = std::make_unique<VideoDecoderVerbatim>();
  } else if (codec == ChannelConfig::CODEC_VP8) {
    decoder_ = VideoDecoderVpx::CreateForVP8();
  } else if (codec == ChannelConfig::CODEC_VP9) {
    decoder_ = VideoDecoderVpx::CreateForVP9();
  } else {
    NOTREACHED() << "Invalid Encoding found: " << codec;
  }

  decoder_->SetPixelFormat(
      (consumer_->GetPixelFormat() == protocol::FrameConsumer::FORMAT_BGRA)
          ? VideoDecoder::PixelFormat::BGRA
          : VideoDecoder::PixelFormat::RGBA);
}

protocol::VideoStub* SoftwareVideoRenderer::GetVideoStub() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return this;
}

protocol::FrameConsumer* SoftwareVideoRenderer::GetFrameConsumer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return consumer_;
}

protocol::FrameStatsConsumer* SoftwareVideoRenderer::GetFrameStatsConsumer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return nullptr;
}

void SoftwareVideoRenderer::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> packet,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::ScopedClosureRunner done_runner(std::move(done));

  // If the video packet is empty then there is nothing to decode. Empty packets
  // are used to maintain activity on the network.
  if (!packet || !packet->has_data() || packet->data().size() == 0) {
    return;
  }

  // The first video packet contains the width and height and subsequent packets
  // contain the dirty rects.
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

  // For the logging statement, we can assume x_dpi and y_dpi are the same.
  // TODO: joedow - Remove logging if the prototype is promoted to production.
  CLIENT_LOG << "Received video packet: " << source_size_.width() << "x"
             << source_size_.height() << "@" << source_dpi_.x() << " for "
             << "encoder: " << packet->format().encoding()
             << " frame_id: " << packet->frame_id()
             << " with dirty_rects_count: " << packet->dirty_rects_size();

  auto frame = consumer_->AllocateFrame(source_size_);
  frame->set_dpi(source_dpi_);

  decode_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DoDecodeFrame, decoder_.get(), std::move(packet),
                     std::move(frame)),
      base::BindOnce(&SoftwareVideoRenderer::RenderFrame,
                     weak_factory_.GetWeakPtr(), done_runner.Release()));
}

void SoftwareVideoRenderer::RenderFrame(
    base::OnceClosure done,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (frame) {
    consumer_->DrawFrame(std::move(frame), std::move(done));
  } else if (done) {
    std::move(done).Run();
  }
}

}  // namespace remoting
