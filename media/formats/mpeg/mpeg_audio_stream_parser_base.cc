// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/mpeg_audio_stream_parser_base.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/byte_queue.h"
#include "media/base/channel_layout.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mpeg/lib.rs.h"

namespace media {

static const int kMpegAudioTrackId = 1;

MPEGAudioStreamParserBase::MPEGAudioStreamParserBase(AudioCodec audio_codec,
                                                     int codec_delay)
    : audio_codec_(audio_codec), codec_delay_(codec_delay) {}

MPEGAudioStreamParserBase::~MPEGAudioStreamParserBase() = default;

void MPEGAudioStreamParserBase::Init(
    InitCB init_cb,
    NewConfigCB config_cb,
    NewBuffersCB new_buffers_cb,
    EncryptedMediaInitDataCB encrypted_media_init_data_cb,
    NewMediaSegmentCB new_segment_cb,
    EndMediaSegmentCB end_of_segment_cb,
    MediaLog* media_log) {
  DVLOG(1) << __func__;
  DCHECK_EQ(state_, UNINITIALIZED);
  init_cb_ = std::move(init_cb);
  config_cb_ = std::move(config_cb);
  new_buffers_cb_ = std::move(new_buffers_cb);
  new_segment_cb_ = std::move(new_segment_cb);
  end_of_segment_cb_ = std::move(end_of_segment_cb);

  ChangeState(INITIALIZED);
}

void MPEGAudioStreamParserBase::Flush() {
  DVLOG(1) << __func__;
  DCHECK_NE(state_, UNINITIALIZED);
  queue_.Reset();
  uninspected_pending_bytes_ = 0;
  if (timestamp_helper_) {
    timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
  }

  in_media_segment_ = false;
}

bool MPEGAudioStreamParserBase::GetGenerateTimestampsFlag() const {
  return true;
}

bool MPEGAudioStreamParserBase::AppendToParseBuffer(
    base::span<const uint8_t> buf) {
  DVLOG(1) << __func__ << "(" << buf.size() << ")";

  DCHECK(!buf.empty());
  DCHECK_NE(state_, UNINITIALIZED);

  if (state_ == PARSE_ERROR) {
    // To preserve previous app-visible behavior in this hopefully
    // never-encountered path, report no failure to caller due to being in
    // invalid underlying state. If caller then proceeds with async parse (via
    // Parse, below), they will get the expected parse failure.  If, instead, we
    // returned false here, then caller would instead tell app QuotaExceededErr
    // synchronous with the app's appendBuffer() call, instead of async decode
    // error during async parse. Since Parse() cannot succeed in kError state,
    // don't even copy `buf` into `queue_` in this case.
    // TODO(crbug.com/40244241): Instrument this path to see if it can be
    // changed to just DCHECK_NE(state_, PARSE_ERROR).
    return true;
  }

  DCHECK_EQ(state_, INITIALIZED);

  // Ensure that we are not still in the middle of iterating Parse calls for
  // previously appended data. May consider changing this to a DCHECK once
  // stabilized, though since impact of proceeding when this condition fails
  // could lead to memory corruption, preferring CHECK.
  CHECK_EQ(uninspected_pending_bytes_, 0);

  if (!queue_.Push(buf)) {
    DVLOG(2) << "AppendToParseBuffer(): Failed to push buf of size "
             << buf.size();
    return false;
  }

  uninspected_pending_bytes_ = base::checked_cast<int>(buf.size());
  return true;
}

StreamParser::ParseStatus MPEGAudioStreamParserBase::Parse(
    int max_pending_bytes_to_inspect) {
  DVLOG(1) << __func__;

  CHECK_GE(max_pending_bytes_to_inspect, 0);

  if (state_ == PARSE_ERROR) {
    return ParseStatus::kFailed;
  }

  DCHECK_EQ(state_, INITIALIZED);
  CHECK_GE(uninspected_pending_bytes_, 0);

  base::span<const uint8_t> queue_span = queue_.Data();
  CHECK_LE(uninspected_pending_bytes_, static_cast<int>(queue_span.size()));

  // First, determine the amount of bytes not yet popped, though already
  // inspected by previous call(s) to Parse().
  const size_t previously_inspected_bytes =
      queue_span.size() - static_cast<size_t>(uninspected_pending_bytes_);

  // Next, allow up to `max_pending_bytes_to_inspect` more of `queue_` contents
  // beyond those previously inspected to be involved in this Parse() call.
  const int new_bytes_to_inspect =
      std::min(max_pending_bytes_to_inspect, uninspected_pending_bytes_);

  const size_t total_bytes_to_inspect =
      previously_inspected_bytes + static_cast<size_t>(new_bytes_to_inspect);

  // Eagerly assume we will successfully inspect these new bytes. Since parse
  // failures are fatal, this is safe because the parser state will be
  // discarded if an error occurs.
  uninspected_pending_bytes_ -= new_bytes_to_inspect;

  base::span<const uint8_t> active_data =
      queue_span.first(total_bytes_to_inspect);

  BufferQueue buffers;
  size_t bytes_to_pop = 0;
  bool end_of_segment = true;
  while (true) {
    auto rust_data = ::rust::Slice<const uint8_t>(active_data);
    auto action = audio_codec_ == AudioCodec::kAAC
                      ? media::formats::mpeg::parse_adts_action(rust_data)
                      : media::formats::mpeg::parse_mp3_action(rust_data);

    bool need_more_data = false;
    size_t bytes_read = 0;
    bool parsed_metadata = true;

    using ActionType = media::formats::mpeg::ActionType;
    switch (action.action_type) {
      case ActionType::NeedMoreData:
        if (action.partial_frame) {
          end_of_segment = false;
        }
        need_more_data = true;
        break;

      case ActionType::Error:
        ChangeState(PARSE_ERROR);
        return ParseStatus::kFailed;

      case ActionType::Skip:
        bytes_read = action.bytes_to_skip;
        parsed_metadata = false;
        break;

      case ActionType::Metadata:
        bytes_read = action.bytes_to_skip;
        parsed_metadata = true;
        break;

      case ActionType::AudioFrame: {
        const Header header = FfiHeaderToHeader(action.header_info);
        if (!ProcessAudioFrame(header, active_data.first(header.frame_size),
                               &buffers)) {
          ChangeState(PARSE_ERROR);
          return ParseStatus::kFailed;
        }
        bytes_read = header.frame_size;
        end_of_segment = true;
        parsed_metadata = false;
        break;
      }
    }

    if (need_more_data) {
      break;
    }

    CHECK_LE(bytes_read, active_data.size());

    if (parsed_metadata && !buffers.empty() && !SendBuffers(&buffers, true)) {
      return ParseStatus::kFailed;
    }

    active_data = active_data.subspan(bytes_read);
    bytes_to_pop += bytes_read;
    end_of_segment = true;
  }

  queue_.Pop(base::checked_cast<int>(bytes_to_pop));

  if (buffers.empty() || SendBuffers(&buffers, end_of_segment)) {
    if (uninspected_pending_bytes_ > 0) {
      return ParseStatus::kSuccessHasMoreData;
    }
    return ParseStatus::kSuccess;
  }

  return ParseStatus::kFailed;
}

void MPEGAudioStreamParserBase::ChangeState(State state) {
  DVLOG(1) << __func__ << "() : " << state_ << " -> " << state;
  state_ = state;
}

bool MPEGAudioStreamParserBase::ProcessAudioFrame(
    const Header& header,
    base::span<const uint8_t> data,
    BufferQueue* buffers) {
  DVLOG(2) << __func__ << "(" << data.size() << ")";

  DVLOG(2) << " sample_rate " << header.sample_rate << " channel_layout "
           << header.channel_layout << " frame_size " << data.size()
           << " sample_count " << header.sample_count;

  if (config_.IsValidConfig() &&
      (config_.samples_per_second() !=
           base::checked_cast<int>(header.sample_rate) ||
       config_.channel_layout() != header.channel_layout)) {
    // Clear config data so that a config change is initiated.
    config_ = AudioDecoderConfig();

    // Send all buffers associated with the previous config.
    if (!buffers->empty() && !SendBuffers(buffers, true)) {
      return false;
    }
  }

  if (!config_.IsValidConfig()) {
    config_.Initialize(audio_codec_, kSampleFormatF32,
                       ChannelLayoutConfig::FromLayout(header.channel_layout),
                       header.sample_rate, header.extra_data,
                       EncryptionScheme::kUnencrypted, base::TimeDelta(),
                       codec_delay_);
    if (audio_codec_ == AudioCodec::kAAC) {
      config_.disable_discard_decoder_delay();
    }

    base::TimeDelta base_timestamp;
    if (timestamp_helper_) {
      base_timestamp = timestamp_helper_->GetTimestamp();
    }

    timestamp_helper_ =
        std::make_unique<AudioTimestampHelper>(header.sample_rate);
    timestamp_helper_->SetBaseTimestamp(base_timestamp);

    auto media_tracks = std::make_unique<MediaTracks>();
    if (config_.IsValidConfig()) {
      media_tracks->AddAudioTrack(config_, true, kMpegAudioTrackId,
                                  MediaTrack::Kind("main"), MediaTrack::Label(),
                                  MediaTrack::Language());
    }
    if (!config_cb_.Run(std::move(media_tracks))) {
      return false;
    }

    if (init_cb_) {
      InitParameters params(kInfiniteDuration);
      params.detected_audio_track_count = 1;
      std::move(init_cb_).Run(params);
    }
  }

  if (header.metadata_frame) {
    return true;
  }

  // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
  // type and allow multiple audio tracks, if applicable. See
  // https://crbug.com/341581.
  scoped_refptr<StreamParserBuffer> buffer = StreamParserBuffer::CopyFrom(
      data, true, DemuxerStream::AUDIO, kMpegAudioTrackId);
  buffer->set_timestamp(timestamp_helper_->GetTimestamp());
  buffer->set_duration(
      timestamp_helper_->GetFrameDuration(header.sample_count));
  buffers->push_back(buffer);

  timestamp_helper_->AddFrames(header.sample_count);

  return true;
}

bool MPEGAudioStreamParserBase::SendBuffers(BufferQueue* buffers,
                                            bool end_of_segment) {
  DCHECK(!buffers->empty());

  if (!in_media_segment_) {
    in_media_segment_ = true;
    new_segment_cb_.Run();
  }

  BufferQueueMap buffer_queue_map;
  buffer_queue_map.insert(std::make_pair(kMpegAudioTrackId, *buffers));
  if (!new_buffers_cb_.Run(buffer_queue_map))
    return false;
  buffers->clear();

  if (end_of_segment) {
    in_media_segment_ = false;
    end_of_segment_cb_.Run();
  }

  timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
  return true;
}

}  // namespace media
