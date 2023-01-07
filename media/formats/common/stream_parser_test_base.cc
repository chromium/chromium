// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/stream_parser_test_base.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
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
      true,
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
  results_stream_.clear();
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
  EXPECT_TRUE(
      AppendDataInPieces(buffer->data(), buffer->data_size(), append_bytes));
  return results_stream_.str();
}

std::string StreamParserTestBase::ParseData(const uint8_t* data,
                                            size_t length) {
  results_stream_.clear();
  EXPECT_TRUE(AppendDataInPieces(data, length, length));
  return results_stream_.str();
}

bool StreamParserTestBase::AppendDataInPieces(const uint8_t* data,
                                              size_t length,
                                              size_t piece_size) {
  const uint8_t* start = data;
  const uint8_t* end = data + length;
  while (start < end) {
    size_t append_size = std::min(piece_size, static_cast<size_t>(end - start));
    if (!parser_->Parse(start, append_size))
      return false;
    start += append_size;
  }
  return true;
}

void StreamParserTestBase::OnInitDone(
    const StreamParser::InitParameters& params) {
  DVLOG(1) << __func__ << "(" << params.duration.InMilliseconds() << ")";
}

bool StreamParserTestBase::OnNewConfig(
    std::unique_ptr<MediaTracks> tracks,
    const StreamParser::TextTrackConfigMap& text_config) {
  DVLOG(1) << __func__ << ": got " << tracks->tracks().size() << " tracks";
  EXPECT_EQ(tracks->tracks().size(), 1u);
  const auto& track = tracks->tracks()[0];
  EXPECT_EQ(track->type(), MediaTrack::Audio);
  audio_track_id_ = track->bytestream_track_id();
  last_audio_config_ = tracks->getAudioConfig(track->bytestream_track_id());
  EXPECT_TRUE(last_audio_config_.IsValidConfig());
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
