// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_stream_parser.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/byte_queue.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser.h"
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
    NewConfigCB config_cb,
    NewBuffersCB new_buffers_cb,
    EncryptedMediaInitDataCB encrypted_media_init_data_cb,
    NewMediaSegmentCB new_segment_cb,
    EndMediaSegmentCB end_of_segment_cb,
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
  config_cb_ = std::move(config_cb);
  new_buffers_cb_ = std::move(new_buffers_cb);
  encrypted_media_init_data_cb_ = std::move(encrypted_media_init_data_cb);
  new_segment_cb_ = std::move(new_segment_cb);
  end_of_segment_cb_ = std::move(end_of_segment_cb);
  media_log_ = media_log;
}

void WebMStreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);

  byte_queue_.Reset();
  uninspected_pending_bytes_ = 0;
  if (cluster_parser_) {
    cluster_parser_->Reset();
  }

  if (state_ == kParsingClusters) {
    ChangeState(kParsingHeaders);
  }
}

bool WebMStreamParser::GetGenerateTimestampsFlag() const {
  return false;
}

bool WebMStreamParser::AppendToParseBuffer(base::span<const uint8_t> buf) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError) {
    // To preserve previous app-visible behavior in this hopefully
    // never-encountered path, report no failure to caller due to being in
    // invalid underlying state. If caller then proceeds with async parse (via
    // Parse, below), they will get the expected parse failure.  If, instead, we
    // returned false here, then caller would instead tell app QuotaExceededErr
    // synchronous with the app's appendBuffer() call, instead of async decode
    // error during async parse. Since Parse() cannot succeed in kError state,
    // don't even copy `buf` into `byte_queue_` in this case.
    // TODO(crbug.com/40244241): Instrument this path to see if it can be
    // changed to just DCHECK_NE(state_, kError).
    return true;
  }

  // Ensure that we are not still in the middle of iterating Parse calls for
  // previously appended data. May consider changing this to a DCHECK once
  // stabilized, though since impact of proceeding when this condition fails
  // could lead to memory corruption, preferring CHECK.
  CHECK_EQ(uninspected_pending_bytes_, 0);

  uninspected_pending_bytes_ = base::checked_cast<int>(buf.size());
  if (!byte_queue_.Push(buf)) {
    DVLOG(2) << "AppendToParseBuffer(): Failed to push buf of size "
             << buf.size();
    return false;
  }

  return true;
}

StreamParser::ParseStatus WebMStreamParser::Parse(
    int max_pending_bytes_to_inspect) {
  DCHECK_NE(state_, kWaitingForInit);
  DCHECK_GE(max_pending_bytes_to_inspect, 0);

  if (state_ == kError) {
    return ParseStatus::kFailed;
  }

  int result = 0;
  int bytes_parsed = 0;
  const uint8_t* cur = nullptr;
  int queue_size = 0;
  byte_queue_.Peek(&cur, &queue_size);

  // First, determine the amount of bytes not yet popped, though already
  // inspected by previous call(s) to Parse().
  int cur_size = queue_size - uninspected_pending_bytes_;
  DCHECK_GE(cur_size, 0);

  // Next, allow up to `max_pending_bytes_to_inspect` more of `byte_queue_`
  // contents beyond those previously inspected to be involved in this Parse()
  // call.
  int inspection_increment =
      std::min(max_pending_bytes_to_inspect, uninspected_pending_bytes_);
  cur_size += inspection_increment;

  // If successfully parsed, remember that we will have inspected this
  // incremental part of `byte_queue_` contents.
  uninspected_pending_bytes_ -= inspection_increment;
  DCHECK_GE(uninspected_pending_bytes_, 0);

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
        return ParseStatus::kFailed;
    }

    if (result < 0) {
      ChangeState(kError);
      return ParseStatus::kFailed;
    }

    if (state_ == oldState && result == 0)
      break;

    DCHECK_GE(result, 0);
    cur += result;
    cur_size -= result;
    bytes_parsed += result;
  }

  byte_queue_.Pop(bytes_parsed);
  if (uninspected_pending_bytes_ > 0) {
    return ParseStatus::kSuccessHasMoreData;
  }
  return ParseStatus::kSuccess;
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
    case kWebMIdCluster:
      if (!cluster_parser_) {
        MEDIA_LOG(ERROR, media_log_) << "Found Cluster element before Info.";
        return -1;
      }
      ChangeState(kParsingClusters);
      new_segment_cb_.Run();
      return 0;
    case kWebMIdSegment:
      // Segment of unknown size indicates live stream.
      if (element_size == kWebMUnknownSize)
        unknown_segment_size_ = true;
      // Just consume the segment header.
      return result;
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

  WebMTracksParser tracks_parser(media_log_);
  result = tracks_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  bytes_parsed += result;

  int64_t timecode_scale_in_ns = info_parser.timecode_scale_ns();
  double timecode_scale_in_us = timecode_scale_in_ns / 1000.0;
  InitParameters params(kInfiniteDuration);

  if (info_parser.duration() > 0) {
    int64_t duration_in_us = info_parser.duration() * timecode_scale_in_us;
    params.duration = base::Microseconds(duration_in_us);
  }

  params.timeline_offset = info_parser.date_utc();

  if (unknown_segment_size_ && (info_parser.duration() <= 0) &&
      !info_parser.date_utc().is_null()) {
    params.liveness = StreamLiveness::kLive;
  } else if (info_parser.duration() >= 0) {
    params.liveness = StreamLiveness::kRecorded;
  } else {
    params.liveness = StreamLiveness::kUnknown;
  }

  const AudioDecoderConfig& audio_config = tracks_parser.audio_decoder_config();
  if (audio_config.is_encrypted())
    OnEncryptedMediaInitData(tracks_parser.audio_encryption_key_id());

  const VideoDecoderConfig& video_config = tracks_parser.video_decoder_config();
  if (video_config.is_encrypted())
    OnEncryptedMediaInitData(tracks_parser.video_encryption_key_id());

  std::unique_ptr<MediaTracks> media_tracks = tracks_parser.media_tracks();
  CHECK(media_tracks.get());
  if (!config_cb_.Run(std::move(media_tracks))) {
    DVLOG(1) << "New config data isn't allowed.";
    return -1;
  }

  cluster_parser_ = std::make_unique<WebMClusterParser>(
      timecode_scale_in_ns, tracks_parser.audio_track_num(),
      tracks_parser.GetAudioDefaultDuration(timecode_scale_in_ns),
      tracks_parser.video_track_num(),
      tracks_parser.GetVideoDefaultDuration(timecode_scale_in_ns),
      tracks_parser.ignored_tracks(), tracks_parser.audio_encryption_key_id(),
      tracks_parser.video_encryption_key_id(), audio_config.codec(),
      media_log_);

  if (init_cb_) {
    params.detected_audio_track_count =
        tracks_parser.detected_audio_track_count();
    params.detected_video_track_count =
        tracks_parser.detected_video_track_count();
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
