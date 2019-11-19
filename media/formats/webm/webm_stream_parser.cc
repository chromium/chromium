// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_stream_parser.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/webm/webm_cluster_parser.h"
#include "media/formats/webm/webm_constants.h"
#include "media/formats/webm/webm_content_encodings.h"
#include "media/formats/webm/webm_info_parser.h"
#include "media/formats/webm/webm_tracks_parser.h"

namespace media {

WebMStreamParser::WebMStreamParser()
    : state_(kWaitingForInit),
      unknown_segment_size_(false) {
}

WebMStreamParser::~WebMStreamParser() = default;

void WebMStreamParser::Init(
    InitCB init_cb,
    const NewConfigCB& config_cb,
    const NewBuffersCB& new_buffers_cb,
    bool ignore_text_tracks,
    const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    const NewMediaSegmentCB& new_segment_cb,
    const EndMediaSegmentCB& end_of_segment_cb,
    MediaLog* media_log) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(!init_cb_);
  DCHECK(init_cb);
  DCHECK(config_cb);
  DCHECK(new_buffers_cb);
  DCHECK(encrypted_media_init_data_cb);
  DCHECK(new_segment_cb);
  DCHECK(end_of_segment_cb);

  ChangeState(kParsingHeaders);
  init_cb_ = std::move(init_cb);
  config_cb_ = config_cb;
  new_buffers_cb_ = new_buffers_cb;
  ignore_text_tracks_ = ignore_text_tracks;
  encrypted_media_init_data_cb_ = encrypted_media_init_data_cb;
  new_segment_cb_ = new_segment_cb;
  end_of_segment_cb_ = end_of_segment_cb;
  media_log_ = media_log;
}

void WebMStreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);

  byte_queue_.Reset();
  if (cluster_parser_)
    cluster_parser_->Reset();
  if (state_ == kParsingClusters)
    ChangeState(kParsingHeaders);
}

bool WebMStreamParser::GetGenerateTimestampsFlag() const {
  return false;
}

bool WebMStreamParser::Parse(const uint8_t* buf, int size) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  byte_queue_.Push(buf, size);

  int result = 0;
  int bytes_parsed = 0;
  const uint8_t* cur = NULL;
  int cur_size = 0;

  byte_queue_.Peek(&cur, &cur_size);
  while (cur_size > 0) {
    State oldState = state_;
    switch (state_) {
      case kParsingHeaders:
        result = ParseInfoAndTracks(cur, cur_size);
        break;

      case kParsingClusters:
        result = ParseCluster(cur, cur_size);
        break;

      case kWaitingForInit:
      case kError:
        return false;
    }

    if (result < 0) {
      ChangeState(kError);
      return false;
    }

    if (state_ == oldState && result == 0)
      break;

    DCHECK_GE(result, 0);
    cur += result;
    cur_size -= result;
    bytes_parsed += result;
  }

  byte_queue_.Pop(bytes_parsed);
  return true;
}

void WebMStreamParser::ChangeState(State new_state) {
  DVLOG(1) << "ChangeState() : " << state_ << " -> " << new_state;
  state_ = new_state;
}

int WebMStreamParser::ParseInfoAndTracks(const uint8_t* data, int size) {
  DVLOG(2) << "ParseInfoAndTracks()";
  DCHECK(data);
  DCHECK_GT(size, 0);

  const uint8_t* cur = data;
  int cur_size = size;
  int bytes_parsed = 0;

  int id;
  int64_t element_size;
  int result = WebMParseElementHeader(cur, cur_size, &id, &element_size);

  if (result <= 0)
    return result;

  switch (id) {
    case kWebMIdEBMLHeader:
    case kWebMIdSeekHead:
    case kWebMIdVoid:
    case kWebMIdCRC32:
    case kWebMIdCues:
    case kWebMIdChapters:
    case kWebMIdTags:
    case kWebMIdAttachments:
      // TODO(matthewjheaney): Implement support for chapters.
      if (cur_size < (result + element_size)) {
        // We don't have the whole element yet. Signal we need more data.
        return 0;
      }
      // Skip the element.
      return result + element_size;
      break;
    case kWebMIdCluster:
      if (!cluster_parser_) {
        MEDIA_LOG(ERROR, media_log_) << "Found Cluster element before Info.";
        return -1;
      }
      ChangeState(kParsingClusters);
      new_segment_cb_.Run();
      return 0;
      break;
    case kWebMIdSegment:
      // Segment of unknown size indicates live stream.
      if (element_size == kWebMUnknownSize)
        unknown_segment_size_ = true;
      // Just consume the segment header.
      return result;
      break;
    case kWebMIdInfo:
      // We've found the element we are looking for.
      break;
    default: {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected element ID 0x" << std::hex
                                   << id;
      return -1;
    }
  }

  WebMInfoParser info_parser;
  result = info_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  cur += result;
  cur_size -= result;
  bytes_parsed += result;

  WebMTracksParser tracks_parser(media_log_, ignore_text_tracks_);
  result = tracks_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  bytes_parsed += result;

  int64_t timecode_scale_in_ns = info_parser.timecode_scale_ns();
  double timecode_scale_in_us = timecode_scale_in_ns / 1000.0;
  InitParameters params(kInfiniteDuration);

  if (info_parser.duration() > 0) {
    int64_t duration_in_us = info_parser.duration() * timecode_scale_in_us;
    params.duration = base::TimeDelta::FromMicroseconds(duration_in_us);
  }

  params.timeline_offset = info_parser.date_utc();

  if (unknown_segment_size_ && (info_parser.duration() <= 0) &&
      !info_parser.date_utc().is_null()) {
    params.liveness = DemuxerStream::LIVENESS_LIVE;
  } else if (info_parser.duration() >= 0) {
    params.liveness = DemuxerStream::LIVENESS_RECORDED;
  } else {
    params.liveness = DemuxerStream::LIVENESS_UNKNOWN;
  }

  const AudioDecoderConfig& audio_config = tracks_parser.audio_decoder_config();
  if (audio_config.is_encrypted())
    OnEncryptedMediaInitData(tracks_parser.audio_encryption_key_id());

  const VideoDecoderConfig& video_config = tracks_parser.video_decoder_config();
  if (video_config.is_encrypted())
    OnEncryptedMediaInitData(tracks_parser.video_encryption_key_id());

  std::unique_ptr<MediaTracks> media_tracks = tracks_parser.media_tracks();
  CHECK(media_tracks.get());
  if (!config_cb_.Run(std::move(media_tracks), tracks_parser.text_tracks())) {
    DVLOG(1) << "New config data isn't allowed.";
    return -1;
  }

  cluster_parser_.reset(new WebMClusterParser(
      timecode_scale_in_ns, tracks_parser.audio_track_num(),
      tracks_parser.GetAudioDefaultDuration(timecode_scale_in_ns),
      tracks_parser.video_track_num(),
      tracks_parser.GetVideoDefaultDuration(timecode_scale_in_ns),
      tracks_parser.text_tracks(), tracks_parser.ignored_tracks(),
      tracks_parser.audio_encryption_key_id(),
      tracks_parser.video_encryption_key_id(), audio_config.codec(),
      media_log_));

  if (init_cb_) {
    params.detected_audio_track_count =
        tracks_parser.detected_audio_track_count();
    params.detected_video_track_count =
        tracks_parser.detected_video_track_count();
    params.detected_text_track_count =
        tracks_parser.detected_text_track_count();
    std::move(init_cb_).Run(params);
  }

  return bytes_parsed;
}

int WebMStreamParser::ParseCluster(const uint8_t* data, int size) {
  if (!cluster_parser_)
    return -1;

  int bytes_parsed = cluster_parser_->Parse(data, size);
  if (bytes_parsed < 0)
    return bytes_parsed;

  BufferQueueMap buffer_queue_map;
  cluster_parser_->GetBuffers(&buffer_queue_map);

  bool cluster_ended = cluster_parser_->cluster_ended();

  if (!buffer_queue_map.empty() && !new_buffers_cb_.Run(buffer_queue_map)) {
    return -1;
  }

  if (cluster_ended) {
    ChangeState(kParsingHeaders);
    end_of_segment_cb_.Run();
  }

  return bytes_parsed;
}

void WebMStreamParser::OnEncryptedMediaInitData(const std::string& key_id) {
  std::vector<uint8_t> key_id_vector(key_id.begin(), key_id.end());
  encrypted_media_init_data_cb_.Run(EmeInitDataType::WEBM, key_id_vector);
}

}  // namespace media
