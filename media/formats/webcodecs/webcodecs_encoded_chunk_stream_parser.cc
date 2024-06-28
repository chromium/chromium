// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webcodecs/webcodecs_encoded_chunk_stream_parser.h"

#include <string>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"

namespace {

// TODO(crbug.com/40155657): Since these must be identical to those generated
// in the SourceBuffer, consider moving these to possibly stream_parser.h.
// Meanwhile, must be kept in sync with similar constexpr in SourceBuffer
// manually.
constexpr media::StreamParser::TrackId kWebCodecsAudioTrackId = 1;
constexpr media::StreamParser::TrackId kWebCodecsVideoTrackId = 2;

}  // namespace

namespace media {

WebCodecsEncodedChunkStreamParser::WebCodecsEncodedChunkStreamParser(
    std::unique_ptr<AudioDecoderConfig> audio_config)
    : state_(kWaitingForInit), audio_config_(std::move(audio_config)) {
  DCHECK(audio_config_ && !video_config_);
}

WebCodecsEncodedChunkStreamParser::WebCodecsEncodedChunkStreamParser(
    std::unique_ptr<VideoDecoderConfig> video_config)
    : state_(kWaitingForInit), video_config_(std::move(video_config)) {
  DCHECK(video_config_ && !audio_config_);
}

WebCodecsEncodedChunkStreamParser::~WebCodecsEncodedChunkStreamParser() =
    default;

void WebCodecsEncodedChunkStreamParser::Init(
    InitCB init_cb,
    NewConfigCB config_cb,
    NewBuffersCB new_buffers_cb,
    EncryptedMediaInitDataCB /* ignored */,
    NewMediaSegmentCB new_segment_cb,
    EndMediaSegmentCB end_of_segment_cb,
    MediaLog* media_log) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(!init_cb_);
  DCHECK(init_cb);
  DCHECK(config_cb);
  DCHECK(new_buffers_cb);
  DCHECK(new_segment_cb);
  DCHECK(end_of_segment_cb);

  ChangeState(kWaitingForConfigEmission);
  init_cb_ = std::move(init_cb);
  config_cb_ = std::move(config_cb);
  new_buffers_cb_ = std::move(new_buffers_cb);
  new_segment_cb_ = std::move(new_segment_cb);
  end_of_segment_cb_ = std::move(end_of_segment_cb);
  media_log_ = media_log;
}

void WebCodecsEncodedChunkStreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);
  if (state_ == kWaitingForEncodedChunks)
    ChangeState(kWaitingForConfigEmission);
}

bool WebCodecsEncodedChunkStreamParser::GetGenerateTimestampsFlag() const {
  return false;
}

bool WebCodecsEncodedChunkStreamParser::AppendToParseBuffer(
    base::span<const uint8_t> /* buf */) {
  // TODO(crbug.com/40155657): Protect against app reaching this (and similer
  // inverse case in other parsers) simply by using the wrong append method on
  // the SourceBuffer. Maybe a better MEDIA_LOG here would be sufficient?  Or
  // instead have the top-level SourceBuffer throw synchronous exception when
  // attempting the wrong append method, without causing parse/decode error?
  NOTREACHED_IN_MIGRATION();  // ProcessChunks() is the method to use instead
                              // for this parser.
  return true;   // Subsequent async Parse failure will occur below.
}

StreamParser::ParseStatus WebCodecsEncodedChunkStreamParser::Parse(
    int /* max_pending_bytes_to_inspect */) {
  // TODO(crbug.com/40155657): Protect against app reaching this (and similer
  // inverse case in other parsers) simply by using the wrong append method on
  // the SourceBuffer. Maybe a better MEDIA_LOG here would be sufficient?  Or
  // instead have the top-level SourceBuffer throw synchronous exception when
  // attempting the wrong append method, without causing parse/decode error?
  NOTREACHED_IN_MIGRATION();  // ProcessChunks() is the method to use instead
                              // for this parser.
  return ParseStatus::kFailed;
}

bool WebCodecsEncodedChunkStreamParser::ProcessChunks(
    std::unique_ptr<BufferQueue> buffer_queue) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  if (state_ == kWaitingForConfigEmission) {
    // Must (still) have only one config. We'll retain ownership.
    // MediaTracks::AddAudio/VideoTrack copies the config.
    DCHECK((audio_config_ && !video_config_) ||
           (video_config_ && !audio_config_));
    auto media_tracks = std::make_unique<MediaTracks>();
    if (audio_config_) {
      media_tracks->AddAudioTrack(*audio_config_, true, kWebCodecsAudioTrackId,
                                  MediaTrack::Kind("main"),
                                  MediaTrack::Label(""),
                                  MediaTrack::Language(""));
    } else if (video_config_) {
      media_tracks->AddVideoTrack(*video_config_, true, kWebCodecsVideoTrackId,
                                  MediaTrack::Kind("main"),
                                  MediaTrack::Label(""),
                                  MediaTrack::Language(""));
    }

    if (!config_cb_.Run(std::move(media_tracks))) {
      ChangeState(kError);
      return false;
    }

    if (init_cb_) {
      InitParameters params(kInfiniteDuration);
      params.liveness = StreamLiveness::kUnknown;
      if (audio_config_)
        params.detected_audio_track_count = 1;
      if (video_config_)
        params.detected_video_track_count = 1;
      std::move(init_cb_).Run(params);
    }

    ChangeState(kWaitingForEncodedChunks);
  }

  DCHECK_EQ(state_, kWaitingForEncodedChunks);

  // All of |buffer_queue| must be of the media type (audio or video)
  // corresponding to the exactly one type of decoder config we have. Otherwise,
  // the caller has provided encoded chunks for the wrong kind of config.
  DemuxerStream::Type expected_type =
      audio_config_ ? DemuxerStream::AUDIO : DemuxerStream::VIDEO;
  for (const auto& it : *buffer_queue) {
    if (it->type() != expected_type) {
      MEDIA_LOG(ERROR, media_log_)
          << "Incorrect EncodedChunk type (audio vs video) appended";
      ChangeState(kError);
      return false;
    }
  }

  // TODO(crbug.com/40155657): Add a different new_buffers_cb type for us to use
  // so that we can just std::move the buffer_queue, and avoid potential issues
  // with out-of-order timestamps in the caller-provided queue that would
  // otherwise cause parse failure in MergeBufferQueues with the current, legacy
  // style of new_buffers_cb that depends on parsers to emit sanely time-ordered
  // groups of frames from *muxed* multi-track bytestreams. FrameProcessor is
  // capable of handling our buffer_queue verbatim.
  BufferQueueMap buffers;
  if (audio_config_)
    buffers.insert(std::make_pair(kWebCodecsAudioTrackId, *buffer_queue));
  else
    buffers.insert(std::make_pair(kWebCodecsVideoTrackId, *buffer_queue));
  new_segment_cb_.Run();
  if (!new_buffers_cb_.Run(buffers))
    return false;
  end_of_segment_cb_.Run();

  return true;
}

void WebCodecsEncodedChunkStreamParser::ChangeState(State new_state) {
  DVLOG(1) << __func__ << ": " << state_ << " -> " << new_state;
  state_ = new_state;
}

}  // namespace media
