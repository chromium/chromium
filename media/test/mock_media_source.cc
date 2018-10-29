// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/mock_media_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
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

MockMediaSource::MockMediaSource(const std::string& filename,
                                 const std::string& mimetype,
                                 size_t initial_append_size,
                                 bool initial_sequence_mode)
    : current_position_(0),
      initial_append_size_(initial_append_size),
      initial_sequence_mode_(initial_sequence_mode),
      mimetype_(mimetype),
      chunk_demuxer_(new ChunkDemuxer(
          base::Bind(&MockMediaSource::DemuxerOpened, base::Unretained(this)),
          base::DoNothing(),
          base::BindRepeating(&MockMediaSource::OnEncryptedMediaInitData,
                              base::Unretained(this)),
          &media_log_)),
      owned_chunk_demuxer_(chunk_demuxer_) {
  file_data_ = ReadTestDataFile(filename);

  if (initial_append_size_ == kAppendWholeFile)
    initial_append_size_ = file_data_->data_size();

  CHECK_GT(initial_append_size_, 0u);
  CHECK_LE(initial_append_size_, file_data_->data_size());
}

MockMediaSource::MockMediaSource(const std::string& filename,
                                 size_t initial_append_size,
                                 bool initial_sequence_mode)
    : MockMediaSource(filename,
                      GetMimeTypeForFile(filename),
                      initial_append_size,
                      initial_sequence_mode) {}

MockMediaSource::MockMediaSource(scoped_refptr<DecoderBuffer> data,
                                 const std::string& mimetype,
                                 size_t initial_append_size,
                                 bool initial_sequence_mode)
    : file_data_(data),
      current_position_(0),
      initial_append_size_(initial_append_size),
      initial_sequence_mode_(initial_sequence_mode),
      mimetype_(mimetype),
      chunk_demuxer_(new ChunkDemuxer(
          base::Bind(&MockMediaSource::DemuxerOpened, base::Unretained(this)),
          base::DoNothing(),
          base::BindRepeating(&MockMediaSource::OnEncryptedMediaInitData,
                              base::Unretained(this)),
          &media_log_)),
      owned_chunk_demuxer_(chunk_demuxer_) {
  if (initial_append_size_ == kAppendWholeFile)
    initial_append_size_ = file_data_->data_size();

  CHECK_GT(initial_append_size_, 0u);
  CHECK_LE(initial_append_size_, file_data_->data_size());
}

MockMediaSource::~MockMediaSource() = default;

std::unique_ptr<Demuxer> MockMediaSource::GetDemuxer() {
  return std::move(owned_chunk_demuxer_);
}

void MockMediaSource::SetAppendWindow(base::TimeDelta timestamp_offset,
                                      base::TimeDelta append_window_start,
                                      base::TimeDelta append_window_end) {
  last_timestamp_offset_ = timestamp_offset;
  append_window_start_ = append_window_start;
  append_window_end_ = append_window_end;
}

void MockMediaSource::Seek(base::TimeDelta seek_time,
                           size_t new_position,
                           size_t seek_append_size) {
  chunk_demuxer_->StartWaitingForSeek(seek_time);

  chunk_demuxer_->ResetParserState(kSourceId, base::TimeDelta(),
                                   kInfiniteDuration, &last_timestamp_offset_);

  CHECK_LT(new_position, file_data_->data_size());
  current_position_ = new_position;

  AppendData(seek_append_size);
}

void MockMediaSource::Seek(base::TimeDelta seek_time) {
  chunk_demuxer_->StartWaitingForSeek(seek_time);
}

void MockMediaSource::SetSequenceMode(bool sequence_mode) {
  CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));
  chunk_demuxer_->SetSequenceMode(kSourceId, sequence_mode);
}

void MockMediaSource::AppendData(size_t size) {
  CHECK(chunk_demuxer_);
  CHECK_LT(current_position_, file_data_->data_size());
  CHECK_LE(current_position_ + size, file_data_->data_size());

  bool success = chunk_demuxer_->AppendData(
      kSourceId, file_data_->data() + current_position_, size,
      append_window_start_, append_window_end_, &last_timestamp_offset_);
  current_position_ += size;

  ASSERT_EQ(expect_append_success_, success);

  if (do_eos_after_next_append_) {
    do_eos_after_next_append_ = false;
    if (success)
      EndOfStream();
  }
}

bool MockMediaSource::AppendAtTime(base::TimeDelta timestamp_offset,
                                   const uint8_t* pData,
                                   int size) {
  CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));
  bool success =
      chunk_demuxer_->AppendData(kSourceId, pData, size, append_window_start_,
                                 append_window_end_, &timestamp_offset);
  last_timestamp_offset_ = timestamp_offset;
  return success;
}

void MockMediaSource::AppendAtTimeWithWindow(
    base::TimeDelta timestamp_offset,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    const uint8_t* pData,
    int size) {
  CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));
  ASSERT_EQ(
      expect_append_success_,
      chunk_demuxer_->AppendData(kSourceId, pData, size, append_window_start,
                                 append_window_end, &timestamp_offset));
  last_timestamp_offset_ = timestamp_offset;
}

void MockMediaSource::SetMemoryLimits(size_t limit_bytes) {
  chunk_demuxer_->SetMemoryLimitsForTest(DemuxerStream::AUDIO, limit_bytes);
  chunk_demuxer_->SetMemoryLimitsForTest(DemuxerStream::VIDEO, limit_bytes);
}

bool MockMediaSource::EvictCodedFrames(base::TimeDelta currentMediaTime,
                                       size_t newDataSize) {
  return chunk_demuxer_->EvictCodedFrames(kSourceId, currentMediaTime,
                                          newDataSize);
}

void MockMediaSource::RemoveRange(base::TimeDelta start, base::TimeDelta end) {
  chunk_demuxer_->Remove(kSourceId, start, end);
}

void MockMediaSource::EndOfStream() {
  chunk_demuxer_->MarkEndOfStream(PIPELINE_OK);
}

void MockMediaSource::UnmarkEndOfStream() {
  chunk_demuxer_->UnmarkEndOfStream();
}

void MockMediaSource::Shutdown() {
  if (!chunk_demuxer_)
    return;
  chunk_demuxer_->ResetParserState(kSourceId, base::TimeDelta(),
                                   kInfiniteDuration, &last_timestamp_offset_);
  chunk_demuxer_->Shutdown();
  chunk_demuxer_ = NULL;
}

void MockMediaSource::DemuxerOpened() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&MockMediaSource::DemuxerOpenedTask, base::Unretained(this)));
}

void MockMediaSource::DemuxerOpenedTask() {
  ChunkDemuxer::Status status = AddId();
  if (status != ChunkDemuxer::kOk) {
    CHECK(demuxer_failure_cb_);
    demuxer_failure_cb_.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }
  chunk_demuxer_->SetTracksWatcher(
      kSourceId, base::Bind(&MockMediaSource::InitSegmentReceived,
                            base::Unretained(this)));

  chunk_demuxer_->SetParseWarningCallback(
      kSourceId,
      base::Bind(&MockMediaSource::OnParseWarningMock, base::Unretained(this)));

  SetSequenceMode(initial_sequence_mode_);
  AppendData(initial_append_size_);
}

ChunkDemuxer::Status MockMediaSource::AddId() {
  std::string type;
  std::string codecs;
  SplitMime(mimetype_, &type, &codecs);
  return chunk_demuxer_->AddId(kSourceId, type, codecs);
}

void MockMediaSource::ChangeType(const std::string& mimetype) {
  chunk_demuxer_->ResetParserState(kSourceId, base::TimeDelta(),
                                   kInfiniteDuration, &last_timestamp_offset_);
  std::string type;
  std::string codecs;
  SplitMime(mimetype, &type, &codecs);
  mimetype_ = mimetype;
  chunk_demuxer_->ChangeType(kSourceId, type, codecs);
}

void MockMediaSource::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  CHECK(!init_data.empty());
  CHECK(encrypted_media_init_data_cb_);
  encrypted_media_init_data_cb_.Run(init_data_type, init_data);
}

void MockMediaSource::InitSegmentReceived(std::unique_ptr<MediaTracks> tracks) {
  CHECK(tracks.get());
  EXPECT_GT(tracks->tracks().size(), 0u);
  CHECK(chunk_demuxer_);
  // Verify that track ids are unique.
  std::set<MediaTrack::Id> track_ids;
  for (const auto& track : tracks->tracks()) {
    EXPECT_EQ(track_ids.end(), track_ids.find(track->id()));
    track_ids.insert(track->id());
  }
  InitSegmentReceivedMock(tracks);
}

}  // namespace media
