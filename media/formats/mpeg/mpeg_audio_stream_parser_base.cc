// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mpeg/mpeg_audio_stream_parser_base.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "media/base/byte_queue.h"
#include "media/base/media_log.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"

namespace media {

static const int kMpegAudioTrackId = 1;

static const uint32_t kICYStartCode = 0x49435920;  // 'ICY '

// Arbitrary upper bound on the size of an IceCast header before it
// triggers an error.
static const int kMaxIcecastHeaderSize = 4096;

static const uint32_t kID3StartCodeMask = 0xffffff00;
static const uint32_t kID3v1StartCode = 0x54414700;  // 'TAG\0'
static const int kID3v1Size = 128;
static const int kID3v1ExtendedSize = 227;
static const uint32_t kID3v2StartCode = 0x49443300;  // 'ID3\0'

static int LocateEndOfHeaders(const uint8_t* buf, int buf_len, int i) {
  bool was_lf = false;
  char last_c = '\0';
  for (; i < buf_len; ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (was_lf)
        return i + 1;
      was_lf = true;
    } else if (c != '\r' || last_c != '\n') {
      was_lf = false;
    }
    last_c = c;
  }
  return -1;
}

MPEGAudioStreamParserBase::MPEGAudioStreamParserBase(uint32_t start_code_mask,
                                                     AudioCodec audio_codec,
                                                     int codec_delay)
    : state_(UNINITIALIZED),
      media_log_(nullptr),
      in_media_segment_(false),
      start_code_mask_(start_code_mask),
      audio_codec_(audio_codec),
      codec_delay_(codec_delay) {}

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
  media_log_ = media_log;

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

  uninspected_pending_bytes_ = base::checked_cast<int>(buf.size());
  if (!queue_.Push(buf)) {
    DVLOG(2) << "AppendToParseBuffer(): Failed to push buf of size "
             << buf.size();
    return false;
  }

  return true;
}

StreamParser::ParseStatus MPEGAudioStreamParserBase::Parse(
    int max_pending_bytes_to_inspect) {
  DVLOG(1) << __func__;

  DCHECK_GE(max_pending_bytes_to_inspect, 0);

  if (state_ == PARSE_ERROR) {
    return ParseStatus::kFailed;
  }

  DCHECK_EQ(state_, INITIALIZED);

  bool end_of_segment = true;
  BufferQueue buffers;

  const uint8_t* data = nullptr;
  int queue_size = 0;
  queue_.Peek(&data, &queue_size);

  // First, determine the amount of bytes not yet popped, though already
  // inspected by previous call(s) to Parse().
  int data_size = queue_size - uninspected_pending_bytes_;
  DCHECK_GE(data_size, 0);

  // Next, allow up to `max_pending_bytes_to_inspect` more of `queue_` contents
  // beyond those previously inspected to be involved in this Parse() call.
  int inspection_increment =
      std::min(max_pending_bytes_to_inspect, uninspected_pending_bytes_);
  data_size += inspection_increment;

  // If successfully parsed, remember that we will have inspected this
  // incremental part of `byte_queue_` contents. Note that parse failures are
  // fatal.
  uninspected_pending_bytes_ -= inspection_increment;
  DCHECK_GE(uninspected_pending_bytes_, 0);

  int bytes_to_pop = 0;
  for (;;) {
    if (data_size < 4)
      break;

    uint32_t start_code =
        data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    int bytes_read = 0;
    bool parsed_metadata = true;
    if ((start_code & start_code_mask_) == start_code_mask_) {
      bytes_read = ParseFrame(data, data_size, &buffers);

      // Only allow the current segment to end if a full frame has been parsed.
      end_of_segment = bytes_read > 0;
      parsed_metadata = false;
    } else if (start_code == kICYStartCode) {
      bytes_read = ParseIcecastHeader(data, data_size);
    } else if ((start_code & kID3StartCodeMask) == kID3v1StartCode) {
      bytes_read = ParseID3v1(data, data_size);
    } else if ((start_code & kID3StartCodeMask) == kID3v2StartCode) {
      bytes_read = ParseID3v2(data, data_size);
    } else {
      bytes_read = FindNextValidStartCode(data, data_size);

      if (bytes_read > 0) {
        DVLOG(1) << "Unexpected start code 0x" << std::hex << start_code;
        DVLOG(1) << "SKIPPING " << bytes_read << " bytes of garbage.";
      }
    }

    CHECK_LE(bytes_read, data_size);

    if (bytes_read < 0) {
      ChangeState(PARSE_ERROR);
      return ParseStatus::kFailed;
    } else if (bytes_read == 0) {
      // Need more data.
      break;
    }

    // Send pending buffers if we have encountered metadata.
    if (parsed_metadata && !buffers.empty() && !SendBuffers(&buffers, true)) {
      return ParseStatus::kFailed;
    }

    CHECK_GE(data_size, bytes_read);
    data_size -= bytes_read;
    data += bytes_read;
    bytes_to_pop += bytes_read;
    end_of_segment = true;
  }

  queue_.Pop(bytes_to_pop);

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

int MPEGAudioStreamParserBase::ParseFrame(const uint8_t* data,
                                          int size,
                                          BufferQueue* buffers) {
  DVLOG(2) << __func__ << "(" << size << ")";

  int sample_rate;
  ChannelLayout channel_layout;
  int frame_size;
  int sample_count;
  bool metadata_frame = false;
  std::vector<uint8_t> extra_data;
  int bytes_read =
      ParseFrameHeader(data, size, &frame_size, &sample_rate, &channel_layout,
                       &sample_count, &metadata_frame, &extra_data);

  if (bytes_read <= 0)
    return bytes_read;

  // Make sure data contains the entire frame.
  if (size < frame_size)
    return 0;

  DVLOG(2) << " sample_rate " << sample_rate << " channel_layout "
           << channel_layout << " frame_size " << frame_size << " sample_count "
           << sample_count;

  if (config_.IsValidConfig() && (config_.samples_per_second() != sample_rate ||
                                  config_.channel_layout() != channel_layout)) {
    // Clear config data so that a config change is initiated.
    config_ = AudioDecoderConfig();

    // Send all buffers associated with the previous config.
    if (!buffers->empty() && !SendBuffers(buffers, true))
      return -1;
  }

  if (!config_.IsValidConfig()) {
    config_.Initialize(audio_codec_, kSampleFormatF32, channel_layout,
                       sample_rate, extra_data, EncryptionScheme::kUnencrypted,
                       base::TimeDelta(), codec_delay_);
    if (audio_codec_ == AudioCodec::kAAC)
      config_.disable_discard_decoder_delay();

    base::TimeDelta base_timestamp;
    if (timestamp_helper_)
      base_timestamp = timestamp_helper_->GetTimestamp();

    timestamp_helper_ = std::make_unique<AudioTimestampHelper>(sample_rate);
    timestamp_helper_->SetBaseTimestamp(base_timestamp);

    std::unique_ptr<MediaTracks> media_tracks(new MediaTracks());
    if (config_.IsValidConfig()) {
      media_tracks->AddAudioTrack(config_, true, kMpegAudioTrackId,
                                  MediaTrack::Kind("main"), MediaTrack::Label(),
                                  MediaTrack::Language());
    }
    if (!config_cb_.Run(std::move(media_tracks))) {
      return -1;
    }

    if (init_cb_) {
      InitParameters params(kInfiniteDuration);
      params.detected_audio_track_count = 1;
      std::move(init_cb_).Run(params);
    }
  }

  if (metadata_frame)
    return frame_size;

  // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
  // type and allow multiple audio tracks, if applicable. See
  // https://crbug.com/341581.
  scoped_refptr<StreamParserBuffer> buffer = StreamParserBuffer::CopyFrom(
      data, frame_size, true, DemuxerStream::AUDIO, kMpegAudioTrackId);
  buffer->set_timestamp(timestamp_helper_->GetTimestamp());
  buffer->set_duration(timestamp_helper_->GetFrameDuration(sample_count));
  buffers->push_back(buffer);

  timestamp_helper_->AddFrames(sample_count);

  return frame_size;
}

int MPEGAudioStreamParserBase::ParseIcecastHeader(const uint8_t* data,
                                                  int size) {
  DVLOG(1) << __func__ << "(" << size << ")";

  if (size < 4)
    return 0;

  if (memcmp("ICY ", data, 4))
    return -1;

  int locate_size = std::min(size, kMaxIcecastHeaderSize);
  int offset = LocateEndOfHeaders(data, locate_size, 4);
  if (offset < 0) {
    if (locate_size == kMaxIcecastHeaderSize) {
      MEDIA_LOG(ERROR, media_log_) << "Icecast header is too large.";
      return -1;
    }

    return 0;
  }

  return offset;
}

int MPEGAudioStreamParserBase::ParseID3v1(const uint8_t* data, int size) {
  DVLOG(1) << __func__ << "(" << size << ")";

  if (size < 4)
    return 0;

  int needed_size = !memcmp(data, "TAG+", 4) ? kID3v1ExtendedSize : kID3v1Size;

  return (size < needed_size) ? 0 : needed_size;
}

int MPEGAudioStreamParserBase::ParseID3v2(const uint8_t* data, int size) {
  DVLOG(1) << __func__ << "(" << size << ")";

  if (size < 10)
    return 0;

  BitReader reader(data, size);
  int32_t id;
  int version;
  uint8_t flags;
  int32_t id3_size;

  if (!reader.ReadBits(24, &id) || !reader.ReadBits(16, &version) ||
      !reader.ReadBits(8, &flags) || !ParseSyncSafeInt(&reader, &id3_size)) {
    return -1;
  }

  int32_t actual_tag_size = 10 + id3_size;

  // Increment size if 'Footer present' flag is set.
  if (flags & 0x10)
    actual_tag_size += 10;

  // Make sure we have the entire tag.
  if (size < actual_tag_size)
    return 0;

  return actual_tag_size;
}

bool MPEGAudioStreamParserBase::ParseSyncSafeInt(BitReader* reader,
                                                 int32_t* value) {
  *value = 0;
  for (int i = 0; i < 4; ++i) {
    uint8_t tmp;
    if (!reader->ReadBits(1, &tmp) || tmp != 0) {
      MEDIA_LOG(ERROR, media_log_) << "ID3 syncsafe integer byte MSb is not 0!";
      return false;
    }

    if (!reader->ReadBits(7, &tmp))
      return false;

    *value <<= 7;
    *value += tmp;
  }

  return true;
}

int MPEGAudioStreamParserBase::FindNextValidStartCode(const uint8_t* data,
                                                      int size) {
  const uint8_t* start = data;
  const uint8_t* end = data + size;

  while (start < end) {
    int bytes_left = end - start;
    const uint8_t* candidate_start_code =
        static_cast<const uint8_t*>(memchr(start, 0xff, bytes_left));

    if (!candidate_start_code)
      return 0;

    bool parse_header_failed = false;
    const uint8_t* sync = candidate_start_code;
    // Try to find 3 valid frames in a row. 3 was selected to decrease
    // the probability of false positives.
    for (int i = 0; i < 3; ++i) {
      int sync_size = end - sync;
      int frame_size;
      int sync_bytes = ParseFrameHeader(sync, sync_size, &frame_size, nullptr,
                                        nullptr, nullptr, nullptr, nullptr);

      if (sync_bytes == 0)
        return 0;

      if (sync_bytes > 0) {
        DCHECK_LE(sync_bytes, sync_size);

        // Skip over this frame so we can check the next one.
        sync += frame_size;

        // Make sure the next frame starts inside the buffer.
        if (sync >= end)
          return 0;
      } else {
        DVLOG(1) << "ParseFrameHeader() " << i << " failed @" << (sync - data);
        parse_header_failed = true;
        break;
      }
    }

    if (parse_header_failed) {
      // One of the frame header parses failed so |candidate_start_code|
      // did not point to the start of a real frame. Move |start| forward
      // so we can find the next candidate.
      start = candidate_start_code + 1;
      continue;
    }

    return candidate_start_code - data;
  }

  return 0;
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
