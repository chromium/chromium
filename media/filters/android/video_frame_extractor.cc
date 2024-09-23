// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/android/video_frame_extractor.h"

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/data_source.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/blocking_url_protocol.h"
#include "media/filters/ffmpeg_bitstream_converter.h"
#include "media/filters/ffmpeg_glue.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/filters/ffmpeg_h265_to_annex_b_bitstream_converter.h"
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/base/android/extract_sps_and_pps.h"
#include "media/filters/ffmpeg_aac_bitstream_converter.h"
#include "media/filters/ffmpeg_h264_to_annex_b_bitstream_converter.h"
#endif

namespace media {

VideoFrameExtractor::VideoFrameExtractor(DataSource* data_source)
    : data_source_(data_source), video_stream_index_(-1) {}

VideoFrameExtractor::~VideoFrameExtractor() = default;

void VideoFrameExtractor::Start(VideoFrameCallback video_frame_callback) {
  video_frame_callback_ = std::move(video_frame_callback);
  protocol_ = std::make_unique<BlockingUrlProtocol>(
      data_source_, base::BindRepeating(&VideoFrameExtractor::OnError,
                                        weak_factory_.GetWeakPtr()));
  glue_ = std::make_unique<FFmpegGlue>(protocol_.get());

  // This will gradually read media data through |data_source_|.
  if (!glue_->OpenContext()) {
    OnError();
    return;
  }

  // Extract the video stream.
  AVFormatContext* format_context = glue_->format_context();
  for (unsigned int i = 0; i < format_context->nb_streams; ++i) {
    AVStream* stream = format_context->streams[i];
    if (!stream)
      continue;
    const AVCodecParameters* codec_parameters = stream->codecpar;
    const AVMediaType codec_type = codec_parameters->codec_type;

    // Pick the first video stream.
    if (codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_ = stream;
      video_stream_index_ = i;
      DCHECK_EQ(video_stream_index_, stream->index);
      break;
    }
  }

  if (!video_stream_) {
    OnError();
    return;
  }

  // Get the config for decoding the video frame later.
  if (!AVStreamToVideoDecoderConfig(video_stream_, &video_config_)) {
    OnError();
    return;
  }

  auto packet = ReadVideoFrame();
  if (!packet) {
    OnError();
    return;
  }

  ConvertPacket(packet.get());
  NotifyComplete(
      std::vector<uint8_t>(packet->data, packet->data + packet->size),
      video_config_);
}

void VideoFrameExtractor::ConvertPacket(AVPacket* packet) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  DCHECK(video_stream_ && video_stream_->codecpar);
  // TODO(xingliu): Create the bitstream converter in an utility function. This
  // logic is shared with FFmpegDemuxer.
  switch (video_stream_->codecpar->codec_id) {
    case AV_CODEC_ID_H264:
      video_config_.SetExtraData(std::vector<uint8_t>());
      bitstream_converter_ =
          std::make_unique<FFmpegH264ToAnnexBBitstreamConverter>(
              video_stream_->codecpar);
      break;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case AV_CODEC_ID_HEVC:
      bitstream_converter_ =
          std::make_unique<FFmpegH265ToAnnexBBitstreamConverter>(
              video_stream_->codecpar);
      break;
#endif
    case AV_CODEC_ID_AAC:
      bitstream_converter_ = std::make_unique<FFmpegAACBitstreamConverter>(
          video_stream_->codecpar);
      break;
    default:
      break;
  }

  if (bitstream_converter_)
    bitstream_converter_->ConvertPacket(packet);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

ScopedAVPacket VideoFrameExtractor::ReadVideoFrame() {
  AVFormatContext* format_context = glue_->format_context();
  auto packet = ScopedAVPacket::Allocate();
  while (av_read_frame(format_context, packet.get()) >= 0) {
    // Skip frames from streams other than video.
    if (packet->stream_index != video_stream_index_)
      continue;

    DCHECK(packet->flags & AV_PKT_FLAG_KEY);
    return packet;
  }
  return {};
}

void VideoFrameExtractor::NotifyComplete(std::vector<uint8_t> encoded_frame,
                                         const VideoDecoderConfig& config) {
  // Return the encoded video key frame.
  DCHECK(video_frame_callback_);
  std::move(video_frame_callback_).Run(true, std::move(encoded_frame), config);
}

void VideoFrameExtractor::OnError() {
  DCHECK(video_frame_callback_);
  std::move(video_frame_callback_)
      .Run(false, std::vector<uint8_t>(), VideoDecoderConfig());
}

}  // namespace media
