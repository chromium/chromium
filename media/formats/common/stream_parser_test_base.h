// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_COMMON_STREAM_PARSER_TEST_BASE_H_
#define MEDIA_FORMATS_COMMON_STREAM_PARSER_TEST_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "media/base/audio_decoder_config.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"

namespace media {

// Test helper for verifying StreamParser behavior.
class StreamParserTestBase {
 public:
  explicit StreamParserTestBase(std::unique_ptr<StreamParser> stream_parser);

  StreamParserTestBase(const StreamParserTestBase&) = delete;
  StreamParserTestBase& operator=(const StreamParserTestBase&) = delete;

  virtual ~StreamParserTestBase();

 protected:
  // Chunks a given parser appropriate file.  Appends |append_bytes| at a time
  // until the file is exhausted.  Returns a coded string representing the
  // segments and timestamps of the extracted frames.
  //
  // The start of each media segment is designated by "NewSegment", similarly
  // the end of each segment by "EndOfSegment".  Segments end when one or more
  // frames are parsed from an append.  If the append contains a partial frame
  // the segment will continue into the next append.
  //
  // Parsed frame(s) are represented as "{ xxK yyK zzK }"  Where xx, yy, and zz
  // are the timestamps in milliseconds of each parsed frame.  For example:
  //
  //     "NewSegment{ 0K 23K 46K }EndOfSegment"
  //     "NewSegment{ 0K }{ 23K }{ 46K }EndOfSegment"
  //     "NewSegment{ 0K }{ 23K }EndOfSegmentNewSegment{ 46K }EndOfSegment"
  //
  std::string ParseFile(const std::string& filename, int append_bytes);

  // Similar to ParseFile() except parses the given |data| in a single append.
  std::string ParseData(base::span<const uint8_t> data);

  // The last AudioDecoderConfig handed to OnNewConfig().
  const AudioDecoderConfig& last_audio_config() const {
    return last_audio_config_;
  }

 private:
  bool AppendAllDataThenParseInPieces(base::span<const uint8_t> data,
                                      size_t piece_size);
  void OnInitDone(const StreamParser::InitParameters& params);
  bool OnNewConfig(std::unique_ptr<MediaTracks> tracks);
  bool OnNewBuffers(const StreamParser::BufferQueueMap& buffer_queue_map);
  void OnKeyNeeded(EmeInitDataType type, const std::vector<uint8_t>& init_data);
  void OnNewSegment();
  void OnEndOfSegment();

  NullMediaLog media_log_;
  std::unique_ptr<StreamParser> parser_;
  std::stringstream results_stream_;
  AudioDecoderConfig last_audio_config_;
  StreamParser::TrackId audio_track_id_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_STREAM_PARSER_TEST_BASE_H_
