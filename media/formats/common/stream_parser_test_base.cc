// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/stream_parser_test_base.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static std::string BufferQueueToString(
    const StreamParser::BufferQueue& buffers) {
  std::stringstream ss;

  ss << "{";
  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end();
       ++itr) {
    ss << " " << (*itr)->timestamp().InMilliseconds();
    if ((*itr)->is_key_frame())
      ss << "K";
  }
  ss << " }";

  return ss.str();
}

StreamParserTestBase::StreamParserTestBase(
    std::unique_ptr<StreamParser> stream_parser)
    : parser_(std::move(stream_parser)) {
  parser_->Init(
      base::BindOnce(&StreamParserTestBase::OnInitDone, base::Unretained(this)),
      base::BindRepeating(&StreamParserTestBase::OnNewConfig,
                          base::Unretained(this)),
      base::BindRepeating(&StreamParserTestBase::OnNewBuffers,
                          base::Unretained(this)),
      base::BindRepeating(&StreamParserTestBase::OnKeyNeeded,
                          base::Unretained(this)),
      base::BindRepeating(&StreamParserTestBase::OnNewSegment,
                          base::Unretained(this)),
      base::BindRepeating(&StreamParserTestBase::OnEndOfSegment,
                          base::Unretained(this)),
      &media_log_);
}

StreamParserTestBase::~StreamParserTestBase() = default;

std::string StreamParserTestBase::ParseFile(const std::string& filename,
                                            int append_bytes) {
  CHECK_GE(append_bytes, 0);

  results_stream_.clear();
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);

  size_t start = 0;
  size_t end = buffer->size();
  do {
    size_t chunk_size = std::min(static_cast<size_t>(append_bytes),
                                 static_cast<size_t>(end - start));
    // Attempt to incrementally parse each appended chunk to test out the
    // parser's internal management of input queue and pending data bytes.
    EXPECT_TRUE(AppendAllDataThenParseInPieces(
        buffer->AsSpan().subspan(start, chunk_size),
        (chunk_size > 7) ? (chunk_size - 7) : chunk_size));
    start += chunk_size;
  } while (start < end);

  return results_stream_.str();
}

std::string StreamParserTestBase::ParseData(base::span<const uint8_t> data) {
  results_stream_.clear();
  EXPECT_TRUE(AppendAllDataThenParseInPieces(data, data.size()));
  return results_stream_.str();
}

bool StreamParserTestBase::AppendAllDataThenParseInPieces(
    base::span<const uint8_t> data,
    size_t piece_size) {
  if (!parser_->AppendToParseBuffer(data)) {
    return false;
  }

  // Also verify that the expected number of pieces is needed to fully parse
  // `data`.
  size_t expected_remaining_data = data.size();
  bool has_more_data = true;

  // A zero-length append still needs a single iteration of parse.
  while (has_more_data) {
    StreamParser::ParseStatus parse_result = parser_->Parse(piece_size);
    if (parse_result == StreamParser::ParseStatus::kFailed) {
      return false;
    }

    has_more_data =
        parse_result == StreamParser::ParseStatus::kSuccessHasMoreData;

    EXPECT_EQ(piece_size < expected_remaining_data, has_more_data);

    if (has_more_data) {
      expected_remaining_data -= piece_size;
    } else {
      EXPECT_EQ(parse_result, StreamParser::ParseStatus::kSuccess);
    }
  }

  return true;
}

void StreamParserTestBase::OnInitDone(
    const StreamParser::InitParameters& params) {
  DVLOG(1) << __func__ << "(" << params.duration.InMilliseconds() << ")";
}

bool StreamParserTestBase::OnNewConfig(std::unique_ptr<MediaTracks> tracks) {
  DVLOG(1) << __func__ << ": got " << tracks->tracks().size() << " tracks";
  EXPECT_EQ(tracks->tracks().size(), 1u);
  const auto& track = tracks->tracks()[0];
  EXPECT_EQ(track->type(), MediaTrack::Type::kAudio);
  audio_track_id_ = track->stream_id();
  last_audio_config_ = tracks->getAudioConfig(track->stream_id());
  EXPECT_TRUE(last_audio_config_.IsValidConfig());
  // This common test utility only ever expects a single audio track in any
  // tested init segment.
  const auto& audio_configs = tracks->GetAudioConfigs();
  EXPECT_EQ(audio_configs.size(), 1u);
  const auto& itr = audio_configs.find(track->stream_id());
  EXPECT_NE(itr, audio_configs.end());
  EXPECT_TRUE(last_audio_config_.Matches(itr->second));
  EXPECT_EQ(tracks->GetVideoConfigs().size(), 0u);
  return true;
}

bool StreamParserTestBase::OnNewBuffers(
    const StreamParser::BufferQueueMap& buffer_queue_map) {
  EXPECT_EQ(1u, buffer_queue_map.size());
  const auto& itr_audio = buffer_queue_map.find(audio_track_id_);
  EXPECT_NE(buffer_queue_map.end(), itr_audio);
  const StreamParser::BufferQueue& audio_buffers = itr_audio->second;
  EXPECT_FALSE(audio_buffers.empty());

  // Ensure that track ids are properly assigned on all emitted buffers.
  for (const auto& buf : audio_buffers) {
    EXPECT_EQ(audio_track_id_, buf->track_id());
  }

  const std::string buffers_str = BufferQueueToString(audio_buffers);
  DVLOG(1) << __func__ << " : " << buffers_str;
  results_stream_ << buffers_str;
  return true;
}

void StreamParserTestBase::OnKeyNeeded(EmeInitDataType type,
                                       const std::vector<uint8_t>& init_data) {
  DVLOG(1) << __func__ << "(" << static_cast<int>(type) << ", "
           << init_data.size() << ")";
}

void StreamParserTestBase::OnNewSegment() {
  DVLOG(1) << __func__;
  results_stream_ << "NewSegment";
}

void StreamParserTestBase::OnEndOfSegment() {
  DVLOG(1) << __func__;
  results_stream_ << "EndOfSegment";
}

}  // namespace media
