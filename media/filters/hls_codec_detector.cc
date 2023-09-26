// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_codec_detector.h"
#include "base/task/bind_post_task.h"
#include "media/formats/mp2t/mp2t_stream_parser.h"
#include "media/formats/mp4/mp4_stream_parser.h"

namespace media {

HlsCodecDetector::~HlsCodecDetector() = default;
HlsCodecDetector::HlsCodecDetector(MediaLog* log, HlsRenditionHost* host)
    : log_(log->Clone()), rendition_host_(host) {
  CHECK(host);
}

void HlsCodecDetector::DetermineContainerOnly(
    std::unique_ptr<HlsDataSourceStream> stream,
    CodecCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_);
  callback_ = std::move(cb);
  parser_ = nullptr;
  rendition_host_->ReadStream(std::move(stream),
                              base::BindOnce(&HlsCodecDetector::OnStreamFetched,
                                             weak_factory_.GetWeakPtr(),
                                             /*container_only=*/true));
}

void HlsCodecDetector::DetermineContainerAndCodec(
    std::unique_ptr<HlsDataSourceStream> stream,
    CodecCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_);
  callback_ = std::move(cb);
  parser_ = nullptr;
  rendition_host_->ReadStream(std::move(stream),
                              base::BindOnce(&HlsCodecDetector::OnStreamFetched,
                                             weak_factory_.GetWeakPtr(),
                                             /*container_only=*/false));
}

void HlsCodecDetector::OnStreamFetched(
    bool container_only,
    HlsDataSourceStreamManager::ReadResult maybe_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(callback_);

  if (!maybe_stream.has_value()) {
    HlsDemuxerStatus status = {HlsDemuxerStatus::Codes::kPlaylistUrlInvalid,
                               std::move(maybe_stream).error()};
    std::move(callback_).Run(std::move(status));
    return;
  }

  auto stream = std::move(maybe_stream).value();
  auto data_size = stream->BytesInBuffer();
  if (!data_size) {
    // If no data came back, then the data source has been exhausted and we
    // have failed to determine a codec.
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  // If this is the first call to `OnStreamFetched`, then `parser_` should be
  // null, and needs to be created. If `DetermineContainer` fails to determine
  // the container being used, then it will call `callback_` and not set
  // `parser_`.
  if (!parser_) {
    DetermineContainer(container_only, stream->AsRawData(), data_size);
  }

  if (!parser_) {
    // `DetermineContainer` MUST execute `callback_` if it fails to create a
    // parser.
    CHECK(!callback_);
    return;
  }

  // `DetermineContainer` MUST set `container_` if it also sets `parser_`.
  CHECK(!container_.empty());

  if (container_only) {
    // If we only want the container, don't parse any data, and just return
    // the container with an empty codec string.
    std::move(callback_).Run(ContainerAndCodecs{
        .container = std::move(container_),
        .codecs = "",
    });
    return;
  }

  if (!parser_->AppendToParseBuffer(stream->AsRawData(), data_size)) {
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  auto parse_result = StreamParser::ParseStatus::kSuccessHasMoreData;
  while (StreamParser::ParseStatus::kSuccessHasMoreData == parse_result) {
    parse_result = parser_->Parse(StreamParser::kMaxPendingBytesPerParse);
    if (!callback_) {
      // The parser has triggered the codec callback and we no longer need to
      // parse data.
      return;
    }
  }

  CHECK(callback_);
  if (StreamParser::ParseStatus::kFailed == parse_result) {
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  CHECK_EQ(StreamParser::ParseStatus::kSuccess, parse_result);
  if (!stream->CanReadMore()) {
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  // If the existing data was parsed but parsing hasn't resulted in a successful
  // detection, keep reading from the data source until it is exhausted. The
  // HLS chunks are usually not too large, and playback will need to read this
  // chunk initially anyway, so fetching the whole thing isn't going to be an
  // issue.
  stream->Flush();
  rendition_host_->ReadStream(
      std::move(stream),
      base::BindOnce(&HlsCodecDetector::OnStreamFetched,
                     weak_factory_.GetWeakPtr(), container_only));
}

void HlsCodecDetector::DetermineContainer(bool container_only,
                                          const uint8_t* data,
                                          size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  constexpr uint8_t kMP4FirstByte = 0x66;
  constexpr uint8_t kMPEGTSFirstByte = 0x47;

  CHECK(callback_);
  CHECK(!parser_);
  CHECK(container_.empty());

  if (!size) {
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  // Supported headers
  const std::vector<uint8_t> mp4 = {kMP4FirstByte, 0x74, 0x79, 0x70,
                                    0x69,          0x73, 0x6F, 0x6D};

  StreamParser::NewConfigCB on_container_configs;
  switch (data[0]) {
    case kMPEGTSFirstByte: {
      container_ = "video/mp2t";
      // The Mp2t parser wants a list of all codecs it's allowed to check for,
      // which means we need one codec for H264, one for AAC, and one for MP3.
      // It doesn't actually care about the codecs details like profile or
      // level, so we can give it the most basic of each type, and it will still
      // find the actual codecs present in the stream.
      const std::string codecs[]{
          "avc1.420000",  // The H264 baseline bitstream codec
          "aac",          // This is sufficient to get the AAC codec.
          "mp3",          // This is sufficient to get the MP3 codec.
      };
      // TODO(crbug/1266991): The mp2t parser isn't able to determine whether
      // aac audio codecs use sbr (aka double samples per second), so the parser
      // will have to be modified in the future to detect that, and provide it
      // so that we can determine it's presence.
      parser_ =
          std::make_unique<mp2t::Mp2tStreamParser>(base::span{codecs}, false);
      on_container_configs = base::BindRepeating(
          &HlsCodecDetector::OnNewConfigMP2T, base::Unretained(this));
      break;
    }
    case kMP4FirstByte: {
      // TODO(crbug/1266991): Android Media Player doesn't currently support
      // the fragmented mp4 playback case. We'd like to get there someday, but
      // it's not on the initial roadmap. The fragmented mp4 container will
      // start with the bytes 0x66 0x74 0x79 0x70 0x69 0x73 0x6F 0x6D, and we
      // can check for that later.
      std::move(callback_).Run(HlsDemuxerStatus::Codes::kUnsupportedContainer);
      return;
    }
    default: {
      std::move(callback_).Run(HlsDemuxerStatus::Codes::kUnsupportedContainer);
      return;
    }
  }

  if (container_only) {
    // Don't initialize the parser when we only care about querying the
    // container.
    return;
  }

  // `this` owns `parser_` and never transfers it, while `parser` owns these
  // callbacks and never transfers them. It's therefore safe to use
  // base::Unretained here, and we actually have to since weak_ptr + repeating
  // callback + return type isn't allowed.
  parser_->Init(base::DoNothingAs<void(const StreamParser::InitParameters&)>(),
                std::move(on_container_configs),
                base::BindRepeating(&HlsCodecDetector::OnNewBuffers,
                                    base::Unretained(this)),
                base::BindRepeating(&HlsCodecDetector::OnEncryptedMediaInit,
                                    base::Unretained(this)),
                base::DoNothing(), base::DoNothing(), log_.get());
}

void HlsCodecDetector::AddCodecToResponse(std::string codec) {
  if (codec_response_ == "") {
    codec_response_ = codec;
  } else {
    codec_response_ = codec_response_ + ", " + codec;
  }
}

void HlsCodecDetector::ParserInit(const StreamParser::InitParameters& params) {}

bool HlsCodecDetector::OnNewConfigMP2T(std::unique_ptr<MediaTracks> tracks) {
  for (const auto& [id, video_config] : tracks->GetVideoConfigs()) {
    if (video_config.codec() != VideoCodec::kH264) {
      HlsDemuxerStatus error = HlsDemuxerStatus::Codes::kUnsupportedCodec;
      std::move(callback_).Run(
          std::move(error).WithData("codec", video_config.codec()));
      return false;
    }
    // Any avc1 codec will do, since the mp2t parser throws all the info away
    // except for the codec type being h264.
    AddCodecToResponse("avc1.420000");
  }

  for (const auto& [id, audio_config] : tracks->GetAudioConfigs()) {
    if (audio_config.codec() == AudioCodec::kAAC) {
      // Just use a dummy codec here for aac. The actual parser doesn't care
      // when we start demuxing for real.
      AddCodecToResponse("mp4a.40.05");
    } else if (audio_config.codec() == AudioCodec::kMP3) {
      AddCodecToResponse("mp3");
    } else {
      HlsDemuxerStatus error = HlsDemuxerStatus::Codes::kUnsupportedCodec;
      std::move(callback_).Run(
          std::move(error).WithData("codec", audio_config.codec()));
      return false;
    }
  }
  return true;
}

bool HlsCodecDetector::OnNewBuffers(
    const StreamParser::BufferQueueMap& buffers) {
  // Buffers come after all the configs, so once we hit the buffers, we can
  // reply to `callback`. Move `codec_reponse_` and `container_` to clear them
  // for the next parse.
  std::move(callback_).Run(ContainerAndCodecs{
      .container = std::move(container_),
      .codecs = std::move(codec_response_),
  });
  return true;
}

void HlsCodecDetector::OnEncryptedMediaInit(EmeInitDataType type,
                                            const std::vector<uint8_t>& data) {
  std::move(callback_).Run(
      HlsDemuxerStatus::Codes::kEncryptedMediaNotSupported);
}

}  // namespace media
