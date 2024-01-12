// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_codec_detector.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/formats/mp2t/mp2t_stream_parser.h"
#include "media/formats/mp4/mp4_stream_parser.h"

namespace media {

HlsCodecDetector::~HlsCodecDetector() = default;
HlsCodecDetectorImpl::~HlsCodecDetectorImpl() = default;
HlsCodecDetectorImpl::HlsCodecDetectorImpl(MediaLog* log,
                                           HlsRenditionHost* host)
    : log_(log->Clone()), rendition_host_(host) {
  CHECK(host);
}

void HlsCodecDetectorImpl::DetermineContainerOnly(
    std::unique_ptr<HlsDataSourceStream> stream,
    CodecCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_);
  callback_ = std::move(cb);
  parser_ = nullptr;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "HLS::ReadChunk", this);
  rendition_host_->ReadStream(
      std::move(stream), base::BindOnce(&HlsCodecDetectorImpl::OnStreamFetched,
                                        weak_factory_.GetWeakPtr(),
                                        /*container_only=*/true));
}

void HlsCodecDetectorImpl::DetermineContainerAndCodec(
    std::unique_ptr<HlsDataSourceStream> stream,
    CodecCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_);
  callback_ = std::move(cb);
  parser_ = nullptr;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "HLS::ReadChunk", this);
  rendition_host_->ReadStream(
      std::move(stream), base::BindOnce(&HlsCodecDetectorImpl::OnStreamFetched,
                                        weak_factory_.GetWeakPtr(),
                                        /*container_only=*/false));
}

void HlsCodecDetectorImpl::PostSuccessToCallback(std::string container,
                                                 std::string codecs) {
  std::move(callback_).Run(ContainerAndCodecs{.container = std::move(container),
                                              .codecs = std::move(codecs)});
}

void HlsCodecDetectorImpl::OnStreamFetched(
    bool container_only,
    HlsDataSourceProvider::ReadResult maybe_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::ReadChunk", this);
  CHECK(callback_);

  if (!maybe_stream.has_value()) {
    HlsDemuxerStatus status = {HlsDemuxerStatus::Codes::kPlaylistUrlInvalid,
                               std::move(maybe_stream).error()};
    std::move(callback_).Run(std::move(status));
    return;
  }

  auto stream = std::move(maybe_stream).value();
  auto data_size = stream->buffer_size();
  if (!data_size) {
    // If no data came back, then the data source has been exhausted and we
    // have failed to determine a codec.
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  // If this is the first call to `OnStreamFetched`, then `parser_` should be
  // null. Determining the container will create a parser for that specific
  // container, if it supported.
  if (!parser_) {
    auto err =
        DetermineContainer(container_only, stream->raw_data(), data_size);
    if (!err.is_ok()) {
      std::move(callback_).Run(std::move(err));
      return;
    }
  }

  // On success, `DetermineContainer` MUST create a parser and set the container
  // type. Additionally, `callback_` must still be valid. If `container_only`
  // is true, the parser is not initialized, and no codecs should provided.
  CHECK(parser_);
  CHECK(callback_);
  CHECK(!container_.empty());
  if (container_only) {
    return PostSuccessToCallback(std::move(container_), "");
  }

  // A failure to append data is not recoverable, unlike a failure to parse.
  if (!parser_->AppendToParseBuffer(stream->raw_data(), data_size)) {
    std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  auto parse_result = StreamParser::ParseStatus::kSuccessHasMoreData;
  while (StreamParser::ParseStatus::kSuccessHasMoreData == parse_result) {
    // Calling `Parse` has the potential to trigger the container configs cb
    // in the parser. The container config cb can trigger a parse failure if
    // something unexpected happens, but the parse result response loses the
    // detected codecs. As a result, the config cb methods we bind in this
    // class directly respond to `callback_` with a more descriptive error.
    // Similarly, `Parse` may also trigger the new buffers cb in the parser.
    // Since buffers always come after configs, the new buffers CB will
    // respond to `callback_` with success. If `callback_` here is unset, there
    // is no more work to do.
    parse_result = parser_->Parse(StreamParser::kMaxPendingBytesPerParse);
    if (!callback_) {
      return;
    }
  }

  // The parser might fail since it's only being given a fragment of the full
  // media content. If the parser has at some point already detected any codecs
  // by the time it fails, we consider that to be successful. If it's truly a
  // parse failure, then that should kill the player later on.
  if (StreamParser::ParseStatus::kFailed == parse_result) {
    if (!codec_response_.length()) {
      std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
      return;
    }
    return PostSuccessToCallback(std::move(container_),
                                 std::move(codec_response_));
  }

  // The first chunk of data might not have contained the entire segment
  // describing the codecs present. If the stream has no more data though,
  // then the bitstream should be considered invalid.
  CHECK_EQ(StreamParser::ParseStatus::kSuccess, parse_result);
  if (stream->CanReadMore()) {
    stream->Clear();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "HLS::ReadChunk", this);
    rendition_host_->ReadStream(
        std::move(stream),
        base::BindOnce(&HlsCodecDetectorImpl::OnStreamFetched,
                       weak_factory_.GetWeakPtr(), container_only));
    return;
  }

  // All the data has no been read, so if there was anything detected, it's time
  // to return it.
  if (codec_response_.length()) {
    return PostSuccessToCallback(std::move(container_),
                                 std::move(codec_response_));
  }

  std::move(callback_).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
}

HlsDemuxerStatus HlsCodecDetectorImpl::DetermineContainer(bool container_only,
                                                          const uint8_t* data,
                                                          size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  constexpr uint8_t kMP4FirstByte = 0x66;
  constexpr uint8_t kMPEGTSFirstByte = 0x47;

  CHECK(!parser_);
  CHECK(container_.empty());

  if (!size) {
    return HlsDemuxerStatus::Codes::kInvalidBitstream;
  }

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
          &HlsCodecDetectorImpl::OnNewConfigMP2T, base::Unretained(this));
      break;
    }
    case kMP4FirstByte: {
      // TODO(crbug/1266991): Android Media Player doesn't currently support
      // the fragmented mp4 playback case. We'd like to get there someday, but
      // it's not on the initial roadmap. The fragmented mp4 container will
      // start with the bytes 0x66 0x74 0x79 0x70 0x69 0x73 0x6F 0x6D, and we
      // can check for that later.
      return HlsDemuxerStatus::Codes::kUnsupportedContainer;
    }
    default: {
      return HlsDemuxerStatus::Codes::kUnsupportedContainer;
    }
  }

  if (container_only) {
    // Don't initialize the parser when we only care about querying the
    // container.
    return OkStatus();
  }

  // `this` owns `parser_` and never transfers it, while `parser` owns these
  // callbacks and never transfers them. It's therefore safe to use
  // base::Unretained here, and we actually have to since weak_ptr + repeating
  // callback + return type isn't allowed.
  parser_->Init(base::DoNothingAs<void(const StreamParser::InitParameters&)>(),
                std::move(on_container_configs),
                base::BindRepeating(&HlsCodecDetectorImpl::OnNewBuffers,
                                    base::Unretained(this)),
                base::BindRepeating(&HlsCodecDetectorImpl::OnEncryptedMediaInit,
                                    base::Unretained(this)),
                base::DoNothing(), base::DoNothing(), log_.get());

  return OkStatus();
}

void HlsCodecDetectorImpl::AddCodecToResponse(std::string codec) {
  if (codec_response_ == "") {
    codec_response_ = codec;
  } else {
    codec_response_ = codec_response_ + ", " + codec;
  }
}

void HlsCodecDetectorImpl::ParserInit(
    const StreamParser::InitParameters& params) {}

bool HlsCodecDetectorImpl::OnNewConfigMP2T(
    std::unique_ptr<MediaTracks> tracks) {
  CHECK(callback_);
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

bool HlsCodecDetectorImpl::OnNewBuffers(
    const StreamParser::BufferQueueMap& buffers) {
  // Buffers come after all the configs, so once we hit the buffers, we can
  // reply to `callback`.
  PostSuccessToCallback(std::move(container_), std::move(codec_response_));
  return true;
}

void HlsCodecDetectorImpl::OnEncryptedMediaInit(
    EmeInitDataType type,
    const std::vector<uint8_t>& data) {
  std::move(callback_).Run(
      HlsDemuxerStatus::Codes::kEncryptedMediaNotSupported);
}

}  // namespace media
