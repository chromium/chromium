// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/media_file_checker.h"

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_decoding_loop.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/blocking_url_protocol.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/file_data_source.h"

namespace media {

namespace {

constexpr int64_t kMaxCheckTimeInSeconds = 5;

void OnMediaFileCheckerError(bool* called) {
  *called = false;
}

struct Decoder {
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> context;
  std::unique_ptr<FFmpegDecodingLoop> loop;
};

}  // namespace

MediaFileChecker::MediaFileChecker(base::File file) : file_(std::move(file)) {}

MediaFileChecker::~MediaFileChecker() = default;

bool MediaFileChecker::Start(base::TimeDelta check_time) {
  media::FileDataSource source;
  if (!source.Initialize(std::move(file_)))
    return false;

  bool read_ok = true;
  media::BlockingUrlProtocol protocol(
      &source, base::BindRepeating(&OnMediaFileCheckerError, &read_ok));
  media::FFmpegGlue glue(&protocol);
  AVFormatContext* format_context = glue.format_context();

  if (!glue.OpenContext())
    return false;

  if (avformat_find_stream_info(format_context, NULL) < 0)
    return false;

  // Remember the codec context for any decodable audio or video streams.
  bool found_streams = false;
  std::vector<Decoder> stream_contexts(format_context->nb_streams);
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecParameters* cp = format_context->streams[i]->codecpar;

    if (cp->codec_type == AVMEDIA_TYPE_AUDIO ||
        cp->codec_type == AVMEDIA_TYPE_VIDEO) {
      auto context = AVStreamToAVCodecContext(format_context->streams[i]);
      if (!context)
        continue;
      const AVCodec* codec = avcodec_find_decoder(cp->codec_id);
      if (codec && avcodec_open2(context.get(), codec, nullptr) >= 0) {
        auto loop = std::make_unique<FFmpegDecodingLoop>(context.get());
        stream_contexts[i] = {std::move(context), std::move(loop)};
        found_streams = true;
      }
    }
  }

  if (!found_streams)
    return false;

  auto packet = ScopedAVPacket::Allocate();
  int result = 0;

  auto do_nothing_cb = base::BindRepeating([](AVFrame*) { return true; });
  const base::TimeTicks deadline =
      base::TimeTicks::Now() +
      std::min(check_time, base::Seconds(kMaxCheckTimeInSeconds));
  do {
    result = av_read_frame(glue.format_context(), packet.get());
    if (result < 0)
      break;

    auto& decoder = stream_contexts[packet->stream_index];
    if (decoder.loop) {
      result = decoder.loop->DecodePacket(packet.get(), do_nothing_cb) ==
                       FFmpegDecodingLoop::DecodeStatus::kOkay
                   ? 0
                   : -1;
    }

    av_packet_unref(packet.get());
  } while (base::TimeTicks::Now() < deadline && read_ok && result >= 0);

  stream_contexts.clear();
  return read_ok && (result == AVERROR_EOF || result >= 0);
}

}  // namespace media
