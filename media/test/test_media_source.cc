// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/test_media_source.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/stream_parser.h"
#include "media/base/test_data_util.h"
#include "media/base/timestamp_constants.h"

namespace {

// Copies parsed type and codecs from |mimetype| into |type| and |codecs|.
// This code assumes that |mimetype| is one of the following forms:
// 1. mimetype without codecs (e.g. audio/mpeg)
// 2. mimetype with codecs (e.g. video/webm; codecs="vorbis,vp8")
void SplitMime(const std::string& mimetype,
               std::string* type,
               std::string* codecs) {
  DCHECK(type);
  DCHECK(codecs);
  size_t semicolon = mimetype.find(";");
  if (semicolon == std::string::npos) {
    *type = mimetype;
    *codecs = "";
    return;
  }

  *type = mimetype.substr(0, semicolon);
  size_t codecs_param_start = mimetype.find("codecs=\"", semicolon);
  CHECK_NE(codecs_param_start, std::string::npos);
  codecs_param_start += 8;  // Skip over the codecs=".
  size_t codecs_param_end = mimetype.find("\"", codecs_param_start);
  CHECK_NE(codecs_param_end, std::string::npos);
  *codecs = mimetype.substr(codecs_param_start,
                            codecs_param_end - codecs_param_start);
}

}  // namespace

namespace media {

constexpr char kSourceId[] = "SourceId";

TestMediaSource::TestMediaSource(const std::string& filename,
                                 const std::string& mimetype,
                                 size_t initial_append_size,
                                 bool initial_sequence_mode)
    : current_position_(0),
      initial_append_size_(initial_append_size),
      initial_sequence_mode_(initial_sequence_mode),
      mimetype_(mimetype),
      chunk_demuxer_(new ChunkDemuxer(
          base::BindOnce(&TestMediaSource::DemuxerOpened,
                         base::Unretained(this)),
          base::DoNothing(),
          base::BindRepeating(&TestMediaSource::OnEncryptedMediaInitData,
                              base::Unretained(this)),
          &media_log_)),
      owned_chunk_demuxer_(chunk_demuxer_) {
  file_data_ = ReadTestDataFile(filename);

  if (initial_append_size_ == kAppendWholeFile)
    initial_append_size_ = file_data_->size();

  CHECK_GT(initial_append_size_, 0u);
  CHECK_LE(initial_append_size_, file_data_->size());
}

TestMediaSource::TestMediaSource(const std::string& filename,
                                 size_t initial_append_size,
                                 bool initial_sequence_mode)
    : TestMediaSource(filename,
                      GetMimeTypeForFile(filename),
                      initial_append_size,
                      initial_sequence_mode) {}

TestMediaSource::TestMediaSource(scoped_refptr<DecoderBuffer> data,
                                 const std::string& mimetype,
                                 size_t initial_append_size,
                                 bool initial_sequence_mode)
    : file_data_(data),
      current_position_(0),
      initial_append_size_(initial_append_size),
      initial_sequence_mode_(initial_sequence_mode),
      mimetype_(mimetype),
      chunk_demuxer_(new ChunkDemuxer(
          base::BindOnce(&TestMediaSource::DemuxerOpened,
                         base::Unretained(this)),
          base::DoNothing(),
          base::BindRepeating(&TestMediaSource::OnEncryptedMediaInitData,
                              base::Unretained(this)),
          &media_log_)),
      owned_chunk_demuxer_(chunk_demuxer_) {
  if (initial_append_size_ == kAppendWholeFile)
    initial_append_size_ = file_data_->size();

  CHECK_GT(initial_append_size_, 0u);
  CHECK_LE(initial_append_size_, file_data_->size());
}

TestMediaSource::~TestMediaSource() = default;

std::unique_ptr<Demuxer> TestMediaSource::GetDemuxer() {
  return std::move(owned_chunk_demuxer_);
}

void TestMediaSource::SetAppendWindow(base::TimeDelta timestamp_offset,
                                      base::TimeDelta append_window_start,
                                      base::TimeDelta append_window_end) {
  last_timestamp_offset_ = timestamp_offset;
  append_window_start_ = append_window_start;
  append_window_end_ = append_window_end;
}

void TestMediaSource::Seek(base::TimeDelta seek_time,
                           size_t new_position,
                           size_t seek_append_size) {
  chunk_demuxer_->StartWaitingForSeek(seek_time);

  chunk_demuxer_->ResetParserState(kSourceId, base::TimeDelta(),
                                   kInfiniteDuration, &last_timestamp_offset_);

  CHECK_LT(new_position, file_data_->size());
  current_position_ = new_position;

  AppendData(seek_append_size);
}

void TestMediaSource::Seek(base::TimeDelta seek_time) {
  chunk_demuxer_->StartWaitingForSeek(seek_time);
}

void TestMediaSource::SetSequenceMode(bool sequence_mode) {
  CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));
  chunk_demuxer_->SetSequenceMode(kSourceId, sequence_mode);
}

void TestMediaSource::AppendData(size_t size) {
  CHECK(chunk_demuxer_);
  CHECK_LT(current_position_, file_data_->size());
  CHECK_LE(current_position_ + size, file_data_->size());

  // Actual append must succeed in these tests, but parse may fail depending on
  // expectations verified later in this method after RunSegmentParserLoop()
  // call(s) are completed.
  ASSERT_TRUE(chunk_demuxer_->AppendToParseBuffer(
      kSourceId, file_data_->AsSpan().subspan(current_position_, size)));

  // Note that large StreamParser::kMaxPendingBytesPerParse makes these just 1
  // iteration frequently.
  bool success = true;
  bool has_more_data = true;
  while (success && has_more_data) {
    StreamParser::ParseStatus parse_result =
        chunk_demuxer_->RunSegmentParserLoop(kSourceId, append_window_start_,
                                             append_window_end_,
                                             &last_timestamp_offset_);
    success = parse_result != StreamParser::ParseStatus::kFailed;
    has_more_data =
        parse_result == StreamParser::ParseStatus::kSuccessHasMoreData;
  }

  current_position_ += size;

  VerifyExpectedAppendResult(success);

  if (do_eos_after_next_append_) {
    do_eos_after_next_append_ = false;
    if (success)
      EndOfStream();
  }
}

bool TestMediaSource::AppendAtTime(base::TimeDelta timestamp_offset,
                                   base::span<const uint8_t> data) {
  CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));

  EXPECT_TRUE(chunk_demuxer_->AppendToParseBuffer(kSourceId, data));

  // Note that large StreamParser::kMaxPendingBytesPerParse makes these just 1
  // iteration frequently.
  bool success = true;
  bool has_more_data = true;
  while (success && has_more_data) {
    StreamParser::ParseStatus parse_result =
        chunk_demuxer_->RunSegmentParserLoop(kSourceId, append_window_start_,
                                             append_window_end_,
                                             &timestamp_offset);
    success = parse_result != StreamParser::ParseStatus::kFailed;
    has_more_data =
        parse_result == StreamParser::ParseStatus::kSuccessHasMoreData;
  }

  last_timestamp_offset_ = timestamp_offset;
  return success;
}

void TestMediaSource::AppendAtTimeWithWindow(
    base::TimeDelta timestamp_offset,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::span<const uint8_t> data) {
  CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));

  EXPECT_TRUE(chunk_demuxer_->AppendToParseBuffer(kSourceId, data));

  // Note that large StreamParser::kMaxPendingBytesPerParse makes these just 1
  // iteration frequently.
  bool success = true;
  bool has_more_data = true;
  while (success && has_more_data) {
    StreamParser::ParseStatus parse_result =
        chunk_demuxer_->RunSegmentParserLoop(kSourceId, append_window_start,
                                             append_window_end,
                                             &timestamp_offset);
    success = parse_result != StreamParser::ParseStatus::kFailed;
    has_more_data =
        parse_result == StreamParser::ParseStatus::kSuccessHasMoreData;
  }

  VerifyExpectedAppendResult(success);
  last_timestamp_offset_ = timestamp_offset;
}

void TestMediaSource::SetMemoryLimits(size_t limit_bytes) {
  chunk_demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, limit_bytes);
  chunk_demuxer_->SetMemoryLimitsForTest(DemuxerStream::VIDEO, limit_bytes);
}

bool TestMediaSource::EvictCodedFrames(base::TimeDelta currentMediaTime,
                                       size_t newDataSize) {
  return chunk_demuxer_->EvictCodedFrames(kSourceId, currentMediaTime,
                                          newDataSize);
}

void TestMediaSource::RemoveRange(base::TimeDelta start, base::TimeDelta end) {
  chunk_demuxer_->Remove(kSourceId, start, end);
}

void TestMediaSource::EndOfStream() {
  chunk_demuxer_->MarkEndOfStream(PIPELINE_OK);
}

void TestMediaSource::UnmarkEndOfStream() {
  chunk_demuxer_->UnmarkEndOfStream();
}

void TestMediaSource::Shutdown() {
  if (!chunk_demuxer_)
    return;
  chunk_demuxer_->ResetParserState(kSourceId, base::TimeDelta(),
                                   kInfiniteDuration, &last_timestamp_offset_);
  chunk_demuxer_->Shutdown();
  chunk_demuxer_ = nullptr;
}

void TestMediaSource::DemuxerOpened() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestMediaSource::DemuxerOpenedTask,
                                base::Unretained(this)));
}

void TestMediaSource::DemuxerOpenedTask() {
  ChunkDemuxer::Status status = AddId();
  if (status != ChunkDemuxer::kOk) {
    CHECK(demuxer_failure_cb_);
    demuxer_failure_cb_.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }
  chunk_demuxer_->SetTracksWatcher(
      kSourceId, base::BindRepeating(&TestMediaSource::InitSegmentReceived,
                                     base::Unretained(this)));

  chunk_demuxer_->SetParseWarningCallback(
      kSourceId, base::BindRepeating(&TestMediaSource::OnParseWarningMock,
                                     base::Unretained(this)));

  SetSequenceMode(initial_sequence_mode_);
  AppendData(initial_append_size_);
}

ChunkDemuxer::Status TestMediaSource::AddId() {
  std::string type;
  std::string codecs;
  SplitMime(mimetype_, &type, &codecs);
  return chunk_demuxer_->AddId(kSourceId, type, codecs);
}

void TestMediaSource::ChangeType(const std::string& mimetype) {
  chunk_demuxer_->ResetParserState(kSourceId, base::TimeDelta(),
                                   kInfiniteDuration, &last_timestamp_offset_);
  std::string type;
  std::string codecs;
  SplitMime(mimetype, &type, &codecs);
  mimetype_ = mimetype;
  chunk_demuxer_->ChangeType(kSourceId, type, codecs);
}

void TestMediaSource::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  CHECK(!init_data.empty());
  CHECK(encrypted_media_init_data_cb_);
  encrypted_media_init_data_cb_.Run(init_data_type, init_data);
}

void TestMediaSource::InitSegmentReceived(std::unique_ptr<MediaTracks> tracks) {
  CHECK(tracks.get());
  EXPECT_GT(tracks->tracks().size(), 0u);
  CHECK(chunk_demuxer_);
  // Verify that track ids are unique.
  std::set<MediaTrack::Id> track_ids;
  for (const auto& track : tracks->tracks()) {
    EXPECT_EQ(track_ids.end(), track_ids.find(track->track_id()));
    track_ids.insert(track->track_id());
  }
  InitSegmentReceivedMock(tracks);
}

void TestMediaSource::VerifyExpectedAppendResult(bool append_result) {
  if (expected_append_result_ == ExpectedAppendResult::kSuccessOrFailure)
    return;  // |append_result| is ignored in this case.

  ASSERT_EQ(expected_append_result_ == ExpectedAppendResult::kSuccess,
            append_result);
}

}  // namespace media
