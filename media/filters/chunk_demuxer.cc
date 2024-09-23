// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/media_tracks.h"
#include "media/base/mime_util.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/frame_processor.h"
#include "media/filters/source_buffer_stream.h"
#include "media/filters/stream_parser_factory.h"

namespace {

// Helper to attempt construction of a StreamParser specific to |content_type|
// and |codecs|.
// TODO(wolenetz): Consider relocating this to StreamParserFactory in
// conjunction with updating StreamParserFactory's isTypeSupported() to also
// parse codecs, rather than require preparsed vector.
std::unique_ptr<media::StreamParser> CreateParserForTypeAndCodecs(
    const std::string& content_type,
    const std::string& codecs,
    media::MediaLog* media_log) {
  std::vector<std::string> parsed_codec_ids;
  media::SplitCodecs(codecs, &parsed_codec_ids);
  return media::StreamParserFactory::Create(content_type, parsed_codec_ids,
                                            media_log);
}

// Helper to calculate the expected codecs parsed from initialization segments
// for a few mime types that have an implicit codec.
std::string ExpectedCodecs(const std::string& content_type,
                           const std::string& codecs) {
  if (codecs == "" && content_type == "audio/aac")
    return "aac";
  if (codecs == "" &&
      (content_type == "audio/mpeg" || content_type == "audio/mp3"))
    return "mp3";
  return codecs;
}

}  // namespace

namespace media {

ChunkDemuxerStream::ChunkDemuxerStream(Type type, MediaTrack::Id media_track_id)
    : type_(type),
      liveness_(StreamLiveness::kUnknown),
      media_track_id_(media_track_id),
      state_(UNINITIALIZED),
      is_enabled_(true) {}

void ChunkDemuxerStream::StartReturningData() {
  DVLOG(1) << "ChunkDemuxerStream::StartReturningData()";
  base::AutoLock auto_lock(lock_);
  DCHECK(!read_cb_);
  ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

void ChunkDemuxerStream::AbortReads() {
  DVLOG(1) << "ChunkDemuxerStream::AbortReads()";
  base::AutoLock auto_lock(lock_);
  ChangeState_Locked(RETURNING_ABORT_FOR_READS);
  if (read_cb_)
    std::move(read_cb_).Run(kAborted, {});
}

void ChunkDemuxerStream::CompletePendingReadIfPossible() {
  base::AutoLock auto_lock(lock_);
  if (!read_cb_)
    return;

  CompletePendingReadIfPossible_Locked();
}

void ChunkDemuxerStream::Shutdown() {
  DVLOG(1) << "ChunkDemuxerStream::Shutdown()";
  base::AutoLock auto_lock(lock_);
  ChangeState_Locked(SHUTDOWN);

  // Pass an end of stream buffer to the pending callback to signal that no more
  // data will be sent.
  if (read_cb_) {
    std::move(read_cb_).Run(DemuxerStream::kOk,
                            {StreamParserBuffer::CreateEOSBuffer()});
  }
}

bool ChunkDemuxerStream::IsSeekWaitingForData() const {
  base::AutoLock auto_lock(lock_);
  return stream_->IsSeekPending();
}

void ChunkDemuxerStream::Seek(base::TimeDelta time) {
  DVLOG(1) << "ChunkDemuxerStream::Seek(" << time.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK(!read_cb_);
  DCHECK(state_ == UNINITIALIZED || state_ == RETURNING_ABORT_FOR_READS)
      << state_;

  stream_->Seek(time);
}

bool ChunkDemuxerStream::Append(const StreamParser::BufferQueue& buffers) {
  if (append_observer_cb_)
    append_observer_cb_.Run(&buffers);

  if (buffers.empty())
    return false;

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, SHUTDOWN);
  stream_->Append(buffers);

  if (read_cb_)
    CompletePendingReadIfPossible_Locked();

  return true;
}

void ChunkDemuxerStream::Remove(base::TimeDelta start,
                                base::TimeDelta end,
                                base::TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->Remove(start, end, duration);
}

bool ChunkDemuxerStream::EvictCodedFrames(base::TimeDelta media_time,
                                          size_t newDataSize) {
  base::AutoLock auto_lock(lock_);

  // If the stream is disabled, then the renderer is not reading from it and
  // thus the read position might be stale. MSE GC algorithm uses the read
  // position to determine when to stop removing data from the front of buffered
  // ranges, so do a Seek in order to update the read position and allow the GC
  // to collect unnecessary data that is earlier than the GOP containing
  // |media_time|.
  if (!is_enabled_)
    stream_->Seek(media_time);

  // |media_time| is allowed to be a little imprecise here. GC only needs to
  // know which GOP currentTime points to.
  return stream_->GarbageCollectIfNeeded(media_time, newDataSize);
}

void ChunkDemuxerStream::OnMemoryPressure(
    base::TimeDelta media_time,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
    bool force_instant_gc) {
  // TODO(sebmarchand): Check if MEMORY_PRESSURE_LEVEL_MODERATE should also be
  // ignored.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  base::AutoLock auto_lock(lock_);
  return stream_->OnMemoryPressure(media_time, memory_pressure_level,
                                   force_instant_gc);
}

void ChunkDemuxerStream::OnSetDuration(base::TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->OnSetDuration(duration);
}

Ranges<base::TimeDelta> ChunkDemuxerStream::GetBufferedRanges(
    base::TimeDelta duration) const {
  base::AutoLock auto_lock(lock_);

  Ranges<base::TimeDelta> range = stream_->GetBufferedTime();

  if (range.size() == 0u)
    return range;

  // Clamp the end of the stream's buffered ranges to fit within the duration.
  // This can be done by intersecting the stream's range with the valid time
  // range.
  Ranges<base::TimeDelta> valid_time_range;
  valid_time_range.Add(range.start(0), range.start(0) + duration);
  return range.IntersectionWith(valid_time_range);
}

base::TimeDelta ChunkDemuxerStream::GetLowestPresentationTimestamp() const {
  base::AutoLock auto_lock(lock_);
  return stream_->GetLowestPresentationTimestamp();
}

base::TimeDelta ChunkDemuxerStream::GetHighestPresentationTimestamp() const {
  base::AutoLock auto_lock(lock_);
  return stream_->GetHighestPresentationTimestamp();
}

base::TimeDelta ChunkDemuxerStream::GetBufferedDuration() const {
  base::AutoLock auto_lock(lock_);
  return stream_->GetBufferedDuration();
}

size_t ChunkDemuxerStream::GetMemoryUsage() const {
  base::AutoLock auto_lock(lock_);
  return stream_->GetMemoryUsage();
}

void ChunkDemuxerStream::OnStartOfCodedFrameGroup(DecodeTimestamp start_dts,
                                                  base::TimeDelta start_pts) {
  DVLOG(2) << "ChunkDemuxerStream::OnStartOfCodedFrameGroup(dts "
           << start_dts.InSecondsF() << ", pts " << start_pts.InSecondsF()
           << ")";

  if (group_start_observer_cb_)
    group_start_observer_cb_.Run(start_dts, start_pts);

  base::AutoLock auto_lock(lock_);
  stream_->OnStartOfCodedFrameGroup(start_pts);
}

bool ChunkDemuxerStream::UpdateAudioConfig(const AudioDecoderConfig& config,
                                           bool allow_codec_change,
                                           MediaLog* media_log) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  if (!stream_) {
    DCHECK_EQ(state_, UNINITIALIZED);
    stream_ = std::make_unique<SourceBufferStream>(config, media_log);
    return true;
  }

  return stream_->UpdateAudioConfig(config, allow_codec_change);
}

bool ChunkDemuxerStream::UpdateVideoConfig(const VideoDecoderConfig& config,
                                           bool allow_codec_change,
                                           MediaLog* media_log) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);

  if (!stream_) {
    DCHECK_EQ(state_, UNINITIALIZED);
    stream_ = std::make_unique<SourceBufferStream>(config, media_log);
    return true;
  }

  return stream_->UpdateVideoConfig(config, allow_codec_change);
}

void ChunkDemuxerStream::MarkEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->MarkEndOfStream();
}

void ChunkDemuxerStream::UnmarkEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->UnmarkEndOfStream();
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(uint32_t count, ReadCB read_cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, UNINITIALIZED);
  DCHECK(!read_cb_);

  read_cb_ = base::BindPostTaskToCurrentDefault(std::move(read_cb));
  requested_buffer_count_ = count;

  if (!is_enabled_) {
    DVLOG(1) << "Read from disabled stream, returning EOS";
    std::move(read_cb_).Run(DemuxerStream::kOk,
                            {StreamParserBuffer::CreateEOSBuffer()});
    return;
  }

  CompletePendingReadIfPossible_Locked();
}

DemuxerStream::Type ChunkDemuxerStream::type() const { return type_; }

StreamLiveness ChunkDemuxerStream::liveness() const {
  base::AutoLock auto_lock(lock_);
  return liveness_;
}

AudioDecoderConfig ChunkDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  // Trying to track down crash. http://crbug.com/715761
  CHECK(stream_);
  return stream_->GetCurrentAudioDecoderConfig();
}

VideoDecoderConfig ChunkDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  // Trying to track down crash. http://crbug.com/715761
  CHECK(stream_);
  return stream_->GetCurrentVideoDecoderConfig();
}

bool ChunkDemuxerStream::SupportsConfigChanges() { return true; }

bool ChunkDemuxerStream::IsEnabled() const {
  base::AutoLock auto_lock(lock_);
  return is_enabled_;
}

void ChunkDemuxerStream::SetEnabled(bool enabled, base::TimeDelta timestamp) {
  base::AutoLock auto_lock(lock_);

  if (enabled == is_enabled_)
    return;

  is_enabled_ = enabled;
  if (enabled) {
    DCHECK(stream_);
    stream_->Seek(timestamp);
  } else if (read_cb_) {
    DVLOG(1) << "Read from disabled stream, returning EOS";
    std::move(read_cb_).Run(kOk, {StreamParserBuffer::CreateEOSBuffer()});
  }
}

void ChunkDemuxerStream::SetStreamMemoryLimit(size_t memory_limit) {
  base::AutoLock auto_lock(lock_);
  stream_->set_memory_limit(memory_limit);
}

void ChunkDemuxerStream::SetLiveness(StreamLiveness liveness) {
  base::AutoLock auto_lock(lock_);
  liveness_ = liveness;
}

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxerStream::ChangeState_Locked() : "
           << "type " << type_
           << " - " << state_ << " -> " << state;
  state_ = state;
}

ChunkDemuxerStream::~ChunkDemuxerStream() = default;

void ChunkDemuxerStream::CompletePendingReadIfPossible_Locked() {
  lock_.AssertAcquired();
  DCHECK(read_cb_);

  switch (state_) {
    case UNINITIALIZED:
      NOTREACHED();
    case RETURNING_ABORT_FOR_READS:
      // Null buffers should be returned in this state since we are waiting
      // for a seek. Any buffers in the SourceBuffer should NOT be returned
      // because they are associated with the seek.
      requested_buffer_count_ = 0;
      std::move(read_cb_).Run(kAborted, {});
      DVLOG(2) << __func__ << ": returning kAborted, type " << type_;
      return;
    case SHUTDOWN:
      requested_buffer_count_ = 0;
      std::move(read_cb_).Run(kOk, {StreamParserBuffer::CreateEOSBuffer()});
      DVLOG(2) << __func__ << ": returning kOk with EOS buffer, type " << type_;
      return;
    case RETURNING_DATA_FOR_READS:
      break;
  }
  DCHECK(state_ == RETURNING_DATA_FOR_READS);

  auto [status, buffers] = GetPendingBuffers_Locked();

  // If the status from |stream_| is kNeedBuffer and there's no buffers,
  // then after ChunkDemuxerStream::Append, try to read data again,
  // 'requested_buffer_count_' does not need to be cleared to 0.
  if (status == SourceBufferStreamStatus::kNeedBuffer && buffers.empty()) {
    return;
  }
  // If the status from |stream_| is kConfigChange, the vector muse be
  // empty, then need to notify new config by running |read_cb_|.
  if (status == SourceBufferStreamStatus::kConfigChange) {
    DCHECK(buffers.empty());
    requested_buffer_count_ = 0;
    std::move(read_cb_).Run(kConfigChanged, std::move(buffers));
    return;
  }
  // Other cases are kOk and just return the buffers.
  DCHECK(!buffers.empty());
  requested_buffer_count_ = 0;
  std::move(read_cb_).Run(kOk, std::move(buffers));
}

std::pair<SourceBufferStreamStatus, DemuxerStream::DecoderBufferVector>
ChunkDemuxerStream::GetPendingBuffers_Locked() {
  lock_.AssertAcquired();
  DemuxerStream::DecoderBufferVector output_buffers;
  for (uint32_t i = 0; i < requested_buffer_count_; ++i) {
    // This aims to avoid send out buffers with different config. To
    // simply the config change handling on renderer(receiver) side, prefer to
    // send out buffers before config change happens.
    if (stream_->IsNextBufferConfigChanged() && !output_buffers.empty()) {
      DVLOG(3) << __func__ << " status=0"
               << ", type=" << type_ << ", req_size=" << requested_buffer_count_
               << ", out_size=" << output_buffers.size();
      return {SourceBufferStreamStatus::kSuccess, std::move(output_buffers)};
    }

    scoped_refptr<StreamParserBuffer> buffer;
    SourceBufferStreamStatus status = stream_->GetNextBuffer(&buffer);
    switch (status) {
      case SourceBufferStreamStatus::kSuccess:
        output_buffers.emplace_back(buffer);
        break;
      case SourceBufferStreamStatus::kNeedBuffer:
        // Return early with calling |read_cb_| if output_buffers has buffers
        // since there is no more readable data.
        DVLOG(3) << __func__ << " status=" << (int)status << ", type=" << type_
                 << ", req_size=" << requested_buffer_count_
                 << ", out_size=" << output_buffers.size();
        return {status, std::move(output_buffers)};
      case SourceBufferStreamStatus::kEndOfStream:
        output_buffers.emplace_back(StreamParserBuffer::CreateEOSBuffer());
        DVLOG(3) << __func__ << " status=" << (int)status << ", type=" << type_
                 << ", req_size=" << requested_buffer_count_
                 << ", out_size=" << output_buffers.size();
        return {status, std::move(output_buffers)};
      case SourceBufferStreamStatus::kConfigChange:
        // Since IsNextBufferConfigChanged has detected config change happen and
        // send out buffers if |output_buffers| has buffer. When confige
        // change actually happen it should be the first time run this |for
        // loop|, i.e. output_buffers should be empty.
        DCHECK(output_buffers.empty());
        DVLOG(3) << __func__ << " status=" << (int)status << ", type=" << type_
                 << ", req_size=" << requested_buffer_count_
                 << ", out_size=" << output_buffers.size();
        return {status, std::move(output_buffers)};
    }
  }

  DCHECK_EQ(output_buffers.size(),
            static_cast<size_t>(requested_buffer_count_));
  DVLOG(3) << __func__ << " status are always kSuccess"
           << ", type=" << type_ << ", req_size=" << requested_buffer_count_
           << ", out_size=" << output_buffers.size();
  return {SourceBufferStreamStatus::kSuccess, std::move(output_buffers)};
}

ChunkDemuxer::ChunkDemuxer(
    base::OnceClosure open_cb,
    base::RepeatingClosure progress_cb,
    EncryptedMediaInitDataCB encrypted_media_init_data_cb,
    MediaLog* media_log)
    : open_cb_(std::move(open_cb)),
      progress_cb_(std::move(progress_cb)),
      encrypted_media_init_data_cb_(std::move(encrypted_media_init_data_cb)),
      media_log_(media_log) {
  DCHECK(open_cb_);
  DCHECK(encrypted_media_init_data_cb_);
  MEDIA_LOG(INFO, media_log_) << GetDisplayName();
}

std::string ChunkDemuxer::GetDisplayName() const {
  return "ChunkDemuxer";
}

DemuxerType ChunkDemuxer::GetDemuxerType() const {
  return DemuxerType::kChunkDemuxer;
}

void ChunkDemuxer::Initialize(DemuxerHost* host,
                              PipelineStatusCallback init_cb) {
  DVLOG(1) << "Initialize()";
  TRACE_EVENT_ASYNC_BEGIN0("media", "ChunkDemuxer::Initialize", this);

  base::OnceClosure open_cb;

  // Locked scope
  {
    base::AutoLock auto_lock(lock_);
    if (state_ == SHUTDOWN) {
      // Init cb must only be run after this method returns, so post.
      init_cb_ = base::BindPostTaskToCurrentDefault(std::move(init_cb));
      RunInitCB_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
      return;
    }

    DCHECK_EQ(state_, WAITING_FOR_INIT);
    host_ = host;
    // Do not post init_cb once this function returns because if there is an
    // error after initialization, the error might be reported before init_cb
    // has a chance to run. This is because ChunkDemuxer::ReportError_Locked
    // directly calls DemuxerHost::OnDemuxerError: crbug.com/633016.
    init_cb_ = std::move(init_cb);

    ChangeState_Locked(INITIALIZING);
    open_cb = std::move(open_cb_);
  }

  std::move(open_cb).Run();
}

void ChunkDemuxer::Stop() {
  DVLOG(1) << "Stop()";
  Shutdown();
}

void ChunkDemuxer::Seek(base::TimeDelta time, PipelineStatusCallback cb) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ")";
  DCHECK(time >= base::TimeDelta());
  TRACE_EVENT_ASYNC_BEGIN0("media", "ChunkDemuxer::Seek", this);

  base::AutoLock auto_lock(lock_);
  DCHECK(!seek_cb_);

  seek_cb_ = base::BindPostTaskToCurrentDefault(std::move(cb));
  if (state_ != INITIALIZED && state_ != ENDED) {
    RunSeekCB_Locked(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  if (cancel_next_seek_) {
    cancel_next_seek_ = false;
    RunSeekCB_Locked(PIPELINE_OK);
    return;
  }

  SeekAllSources(time);
  StartReturningData();

  if (IsSeekWaitingForData_Locked()) {
    DVLOG(1) << "Seek() : waiting for more data to arrive.";
    return;
  }

  RunSeekCB_Locked(PIPELINE_OK);
}

bool ChunkDemuxer::IsSeekable() const {
  return true;
}

// Demuxer implementation.
base::Time ChunkDemuxer::GetTimelineOffset() const {
  return timeline_offset_;
}

std::vector<DemuxerStream*> ChunkDemuxer::GetAllStreams() {
  base::AutoLock auto_lock(lock_);
  std::vector<DemuxerStream*> result;
  // Put enabled streams at the beginning of the list so that
  // MediaResource::GetFirstStream returns the enabled stream if there is one.
  // TODO(servolk): Revisit this after media track switching is supported.
  for (const auto& stream : audio_streams_) {
    if (stream->IsEnabled())
      result.push_back(stream.get());
  }
  for (const auto& stream : video_streams_) {
    if (stream->IsEnabled())
      result.push_back(stream.get());
  }
  // Put disabled streams at the end of the vector.
  for (const auto& stream : audio_streams_) {
    if (!stream->IsEnabled())
      result.push_back(stream.get());
  }
  for (const auto& stream : video_streams_) {
    if (!stream->IsEnabled())
      result.push_back(stream.get());
  }
  return result;
}

base::TimeDelta ChunkDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

int64_t ChunkDemuxer::GetMemoryUsage() const {
  base::AutoLock auto_lock(lock_);
  int64_t mem = 0;
  for (const auto& s : audio_streams_)
    mem += s->GetMemoryUsage();
  for (const auto& s : video_streams_)
    mem += s->GetMemoryUsage();
  return mem;
}

std::optional<container_names::MediaContainerName>
ChunkDemuxer::GetContainerForMetrics() const {
  return std::nullopt;
}

void ChunkDemuxer::AbortPendingReads() {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN ||
         state_ == PARSE_ERROR)
      << state_;

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  AbortPendingReads_Locked();
}

void ChunkDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  DVLOG(1) << "StartWaitingForSeek()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN ||
         state_ == PARSE_ERROR) << state_;
  DCHECK(!seek_cb_);

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  AbortPendingReads_Locked();
  SeekAllSources(seek_time);

  // Cancel state set in CancelPendingSeek() since we want to
  // accept the next Seek().
  cancel_next_seek_ = false;
}

void ChunkDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, INITIALIZING);
  DCHECK(!seek_cb_ || IsSeekWaitingForData_Locked());

  if (cancel_next_seek_)
    return;

  AbortPendingReads_Locked();
  SeekAllSources(seek_time);

  if (!seek_cb_) {
    cancel_next_seek_ = true;
    return;
  }

  RunSeekCB_Locked(PIPELINE_OK);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(
    const std::string& id,
    std::unique_ptr<AudioDecoderConfig> audio_config) {
  DCHECK(audio_config);
  DVLOG(1) << __func__ << " id="
           << " audio_config=" << audio_config->AsHumanReadableString();
  base::AutoLock auto_lock(lock_);

  // Any valid audio config provided by WC is bufferable here, though decode
  // error may occur later.
  if (!audio_config->IsValidConfig())
    return ChunkDemuxer::kNotSupported;

  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) ||
      IsValidId_Locked(id)) {
    return kReachedIdLimit;
  }

  DCHECK(init_cb_);

  std::string expected_codec = GetCodecName(audio_config->codec());
  std::unique_ptr<media::StreamParser> stream_parser(
      media::StreamParserFactory::Create(std::move(audio_config)));
  DCHECK(stream_parser);

  return AddIdInternal(id, std::move(stream_parser), expected_codec);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(
    const std::string& id,
    std::unique_ptr<VideoDecoderConfig> video_config) {
  DCHECK(video_config);
  DVLOG(1) << __func__ << " id="
           << " video_config=" << video_config->AsHumanReadableString();
  base::AutoLock auto_lock(lock_);

  // Any valid video config provided by WC is bufferable here, though decode
  // error may occur later.
  if (!video_config->IsValidConfig())
    return ChunkDemuxer::kNotSupported;

  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) ||
      IsValidId_Locked(id)) {
    return kReachedIdLimit;
  }

  DCHECK(init_cb_);

  std::string expected_codec = GetCodecName(video_config->codec());
  std::unique_ptr<media::StreamParser> stream_parser(
      media::StreamParserFactory::Create(std::move(video_config)));
  DCHECK(stream_parser);

  return AddIdInternal(id, std::move(stream_parser), expected_codec);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& content_type,
                                         const std::string& codecs) {
  DVLOG(1) << __func__ << " id=" << id << " content_type=" << content_type
           << " codecs=" << codecs;
  base::AutoLock auto_lock(lock_);

  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) ||
      IsValidId_Locked(id)) {
    return kReachedIdLimit;
  }

  // TODO(wolenetz): Change to DCHECK once less verification in release build is
  // needed. See https://crbug.com/786975.
  CHECK(init_cb_);

  std::unique_ptr<media::StreamParser> stream_parser(
      CreateParserForTypeAndCodecs(content_type, codecs, media_log_));
  if (!stream_parser) {
    DVLOG(1) << __func__ << " failed: unsupported content_type=" << content_type
             << " codecs=" << codecs;
    return ChunkDemuxer::kNotSupported;
  }

  return AddIdInternal(id, std::move(stream_parser),
                       ExpectedCodecs(content_type, codecs));
}

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
ChunkDemuxer::Status ChunkDemuxer::AddAutoDetectedCodecsId(
    const std::string& id,
    RelaxedParserSupportedType mime_type) {
  DVLOG(1) << __func__ << " id=" << id
           << " content_type=" << static_cast<int>(mime_type);
  base::AutoLock auto_lock(lock_);
  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) ||
      IsValidId_Locked(id)) {
    return kReachedIdLimit;
  }

  CHECK(init_cb_);

  std::unique_ptr<media::StreamParser> stream_parser =
      StreamParserFactory::CreateRelaxedParser(mime_type);
  if (!stream_parser) {
    DVLOG(1) << __func__ << " failed: unsupported mime type for relaxed parser";
    return kNotSupported;
  }

  return AddIdInternal(id, std::move(stream_parser), std::nullopt);
}
#endif

ChunkDemuxer::Status ChunkDemuxer::AddIdInternal(
    const std::string& id,
    std::unique_ptr<media::StreamParser> stream_parser,
    std::optional<std::string_view> expected_codecs) {
  DVLOG(2) << __func__ << " id=" << id
           << " expected_codecs=" << expected_codecs.value_or("None");
  lock_.AssertAcquired();

  std::unique_ptr<FrameProcessor> frame_processor =
      std::make_unique<FrameProcessor>(
          base::BindRepeating(&ChunkDemuxer::IncreaseDurationIfNecessary,
                              base::Unretained(this)),
          media_log_);

  std::unique_ptr<SourceBufferState> source_state =
      std::make_unique<SourceBufferState>(
          std::move(stream_parser), std::move(frame_processor),
          base::BindRepeating(&ChunkDemuxer::CreateDemuxerStream,
                              base::Unretained(this), id),
          media_log_);

  // TODO(wolenetz): Change these to DCHECKs or switch to returning
  // kReachedIdLimit once less verification in release build is needed. See
  // https://crbug.com/786975.
  CHECK(pending_source_init_ids_.find(id) == pending_source_init_ids_.end());
  auto insert_result = pending_source_init_ids_.insert(id);
  CHECK(insert_result.first != pending_source_init_ids_.end());
  CHECK(*insert_result.first == id);
  CHECK(insert_result.second);  // Only true if insertion succeeded.

  source_state->Init(base::BindOnce(&ChunkDemuxer::OnSourceInitDone,
                                    base::Unretained(this), id),
                     expected_codecs, encrypted_media_init_data_cb_);

  // TODO(wolenetz): Change to DCHECKs once less verification in release build
  // is needed. See https://crbug.com/786975.
  CHECK(!IsValidId_Locked(id));
  source_state_map_[id] = std::move(source_state);
  CHECK(IsValidId_Locked(id));
  return kOk;
}

void ChunkDemuxer::SetTracksWatcher(const std::string& id,
                                    MediaTracksUpdatedCB tracks_updated_cb) {
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId_Locked(id));
  source_state_map_[id]->SetTracksWatcher(std::move(tracks_updated_cb));
}

void ChunkDemuxer::SetParseWarningCallback(
    const std::string& id,
    SourceBufferParseWarningCB parse_warning_cb) {
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId_Locked(id));
  source_state_map_[id]->SetParseWarningCallback(std::move(parse_warning_cb));
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  DVLOG(1) << __func__ << " id=" << id;
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId_Locked(id));

  source_state_map_.erase(id);
  pending_source_init_ids_.erase(id);
  // Remove demuxer streams created for this id.
  for (const ChunkDemuxerStream* s : id_to_streams_map_[id]) {
    bool stream_found = false;
    for (size_t i = 0; i < audio_streams_.size(); ++i) {
      if (audio_streams_[i].get() == s) {
        stream_found = true;
        removed_streams_.push_back(std::move(audio_streams_[i]));
        audio_streams_.erase(audio_streams_.begin() + i);
        break;
      }
    }
    if (stream_found)
      continue;
    for (size_t i = 0; i < video_streams_.size(); ++i) {
      if (video_streams_[i].get() == s) {
        stream_found = true;
        removed_streams_.push_back(std::move(video_streams_[i]));
        video_streams_.erase(video_streams_.begin() + i);
        break;
      }
    }
    CHECK(stream_found);
  }
  id_to_streams_map_.erase(id);
}

Ranges<base::TimeDelta> ChunkDemuxer::GetBufferedRanges(
    const std::string& id) const {
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());

  auto itr = source_state_map_.find(id);

  CHECK(itr != source_state_map_.end(), base::NotFatalUntil::M130);
  return itr->second->GetBufferedRanges(duration_, state_ == ENDED);
}

base::TimeDelta ChunkDemuxer::GetLowestPresentationTimestamp(
    const std::string& id) const {
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());

  auto itr = source_state_map_.find(id);

  CHECK(itr != source_state_map_.end(), base::NotFatalUntil::M130);
  return itr->second->GetLowestPresentationTimestamp();
}

base::TimeDelta ChunkDemuxer::GetHighestPresentationTimestamp(
    const std::string& id) const {
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());

  auto itr = source_state_map_.find(id);

  CHECK(itr != source_state_map_.end(), base::NotFatalUntil::M130);
  return itr->second->GetHighestPresentationTimestamp();
}

void ChunkDemuxer::FindAndEnableProperTracks(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    DemuxerStream::Type track_type,
    TrackChangeCB change_completed_cb) {
  base::AutoLock auto_lock(lock_);

  std::set<ChunkDemuxerStream*> enabled_streams;
  for (const auto& id : track_ids) {
    auto it = track_id_to_demux_stream_map_.find(id);
    if (it == track_id_to_demux_stream_map_.end())
      continue;
    ChunkDemuxerStream* stream = it->second;
    DCHECK(stream);
    DCHECK_EQ(track_type, stream->type());
    // TODO(servolk): Remove after multiple enabled audio tracks are supported
    // by the media::RendererImpl.
    if (!enabled_streams.empty()) {
      MEDIA_LOG(INFO, media_log_)
          << "Only one enabled track is supported, ignoring track " << id;
      continue;
    }
    enabled_streams.insert(stream);
    stream->SetEnabled(true, curr_time);
  }

  bool is_audio = track_type == DemuxerStream::AUDIO;
  for (const auto& stream : is_audio ? audio_streams_ : video_streams_) {
    if (stream && enabled_streams.find(stream.get()) == enabled_streams.end()) {
      DVLOG(1) << __func__ << ": disabling stream " << stream.get();
      stream->SetEnabled(false, curr_time);
    }
  }

  std::vector<DemuxerStream*> streams(enabled_streams.begin(),
                                      enabled_streams.end());
  std::move(change_completed_cb).Run(streams);
}

void ChunkDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  FindAndEnableProperTracks(track_ids, curr_time, DemuxerStream::AUDIO,
                            std::move(change_completed_cb));
}

void ChunkDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  FindAndEnableProperTracks(track_ids, curr_time, DemuxerStream::VIDEO,
                            std::move(change_completed_cb));
}

void ChunkDemuxer::DisableCanChangeType() {
  supports_change_type_ = false;
}

void ChunkDemuxer::OnMemoryPressure(
    base::TimeDelta currentMediaTime,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
    bool force_instant_gc) {
  // TODO(sebmarchand): Check if MEMORY_PRESSURE_LEVEL_MODERATE should also be
  // ignored.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  base::AutoLock auto_lock(lock_);
  for (const auto& [source, state] : source_state_map_) {
    state->OnMemoryPressure(currentMediaTime, memory_pressure_level,
                            force_instant_gc);
  }
}

bool ChunkDemuxer::EvictCodedFrames(const std::string& id,
                                    base::TimeDelta currentMediaTime,
                                    size_t newDataSize) {
  DVLOG(1) << __func__ << "(" << id << ")"
           << " media_time=" << currentMediaTime.InSecondsF()
           << " newDataSize=" << newDataSize;
  base::AutoLock auto_lock(lock_);

  DCHECK(!id.empty());
  auto itr = source_state_map_.find(id);
  if (itr == source_state_map_.end()) {
    LOG(WARNING) << __func__ << " stream " << id << " not found";
    return false;
  }
  return itr->second->EvictCodedFrames(currentMediaTime, newDataSize);
}

bool ChunkDemuxer::AppendToParseBuffer(const std::string& id,
                                       base::span<const uint8_t> data) {
  DVLOG(1) << "AppendToParseBuffer(" << id << ", " << data.size() << ")";

  DCHECK(!id.empty());

  if (data.empty()) {
    // We don't DCHECK that |state_| != ENDED here, since |state_| is protected
    // by |lock_|. However, transition into ENDED can happen only on
    // MarkEndOfStream called by the MediaSource object on parse failure or on
    // app calling endOfStream(). In case that contract is violated for
    // nonzero-length appends, we still DCHECK within the lock, below.
    return true;
  }

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, ENDED);

    switch (state_) {
      case INITIALIZING:
      case INITIALIZED:
        DCHECK(IsValidId_Locked(id));
        if (!source_state_map_[id]->AppendToParseBuffer(data)) {
          // Just indicate that the append failed. Let the caller give app an
          // error so that it may adapt. This is different from
          // RunSegmentParserLoop(), where fatal MediaSource failure should
          // occur if the underlying parse fails.
          return false;
        }
        break;

      case PARSE_ERROR:
      case WAITING_FOR_INIT:
      case ENDED:
      case SHUTDOWN:
        DVLOG(1) << "AppendToParseBuffer(): called in unexpected state "
                 << state_;
        // To preserve previous app-visible behavior in this hopefully
        // never-encountered path, report no failure to caller due to being in
        // invalid underlying state. If caller then proceeds with async parse
        // (via RunSegmentParserLoop, below), they will get the expected parse
        // failure for this set of states. If, instead, we returned false here,
        // then caller would instead tell app QuotaExceededErr synchronous with
        // the app's appendBuffer() call, instead of async decode error during
        // async parse.
        // TODO(crbug.com/40244241): Instrument this path to see if it can be
        // changed to just NOTREACHED() << state_.
        return true;
    }
  }

  return true;
}

StreamParser::ParseStatus ChunkDemuxer::RunSegmentParserLoop(
    const std::string& id,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  DVLOG(1) << "RunSegmentParserLoop(" << id << ")";

  DCHECK(!id.empty());
  DCHECK(timestamp_offset);

  Ranges<base::TimeDelta> ranges;

  StreamParser::ParseStatus result = StreamParser::ParseStatus::kFailed;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, ENDED);

    // Capture if any of the SourceBuffers are waiting for data before we start
    // parsing.
    bool old_waiting_for_data = IsSeekWaitingForData_Locked();

    switch (state_) {
      case INITIALIZING:
      case INITIALIZED:
        DCHECK(IsValidId_Locked(id));
        result = source_state_map_[id]->RunSegmentParserLoop(
            append_window_start, append_window_end, timestamp_offset);
        if (result == StreamParser::ParseStatus::kFailed) {
          ReportError_Locked(CHUNK_DEMUXER_ERROR_APPEND_FAILED);
          return result;
        }
        break;

      case PARSE_ERROR:
      case WAITING_FOR_INIT:
      case ENDED:
      case SHUTDOWN:
        DVLOG(1) << "RunSegmentParserLoop(): called in unexpected state "
                 << state_;
        return StreamParser::ParseStatus::kFailed;
    }

    // Check to see if newly parsed data was at the pending seek point. This
    // indicates we have parsed enough data to complete the seek. Work is still
    // in progress at this point, but it's okay since |seek_cb_| will post.
    if (old_waiting_for_data && !IsSeekWaitingForData_Locked() && seek_cb_) {
      RunSeekCB_Locked(PIPELINE_OK);
    }

    ranges = GetBufferedRanges_Locked();
  }

  DCHECK_NE(StreamParser::ParseStatus::kFailed, result);
  host_->OnBufferedTimeRangesChanged(ranges);
  progress_cb_.Run();
  return result;
}

bool ChunkDemuxer::AppendChunks(
    const std::string& id,
    std::unique_ptr<StreamParser::BufferQueue> buffer_queue,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  DCHECK(buffer_queue);
  DVLOG(1) << __func__ << ": " << id
           << ", buffer_queue size()=" << buffer_queue->size();

  DCHECK(!id.empty());
  DCHECK(timestamp_offset);

  Ranges<base::TimeDelta> ranges;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, ENDED);

    // Capture if any of the SourceBuffers are waiting for data before we start
    // buffering new chunks.
    bool old_waiting_for_data = IsSeekWaitingForData_Locked();

    if (buffer_queue->size() == 0u)
      return true;

    switch (state_) {
      case INITIALIZING:
      case INITIALIZED:
        DCHECK(IsValidId_Locked(id));
        if (!source_state_map_[id]->AppendChunks(
                std::move(buffer_queue), append_window_start, append_window_end,
                timestamp_offset)) {
          ReportError_Locked(CHUNK_DEMUXER_ERROR_APPEND_FAILED);
          return false;
        }
        break;

      case PARSE_ERROR:
      case WAITING_FOR_INIT:
      case ENDED:
      case SHUTDOWN:
        DVLOG(1) << "AppendChunks(): called in unexpected state " << state_;
        return false;
    }

    // Check to see if data was appended at the pending seek point. This
    // indicates we have parsed enough data to complete the seek. Work is still
    // in progress at this point, but it's okay since |seek_cb_| will post.
    if (old_waiting_for_data && !IsSeekWaitingForData_Locked() && seek_cb_)
      RunSeekCB_Locked(PIPELINE_OK);

    ranges = GetBufferedRanges_Locked();
  }

  host_->OnBufferedTimeRangesChanged(ranges);
  progress_cb_.Run();
  return true;
}

void ChunkDemuxer::ResetParserState(const std::string& id,
                                    base::TimeDelta append_window_start,
                                    base::TimeDelta append_window_end,
                                    base::TimeDelta* timestamp_offset) {
  DVLOG(1) << "ResetParserState(" << id << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());
  CHECK(IsValidId_Locked(id));
  bool old_waiting_for_data = IsSeekWaitingForData_Locked();
  source_state_map_[id]->ResetParserState(append_window_start,
                                          append_window_end,
                                          timestamp_offset);
  // ResetParserState can possibly emit some buffers.
  // Need to check whether seeking can be completed.
  if (old_waiting_for_data && !IsSeekWaitingForData_Locked() && seek_cb_)
    RunSeekCB_Locked(PIPELINE_OK);
}

void ChunkDemuxer::Remove(const std::string& id,
                          base::TimeDelta start,
                          base::TimeDelta end) {
  DVLOG(1) << "Remove(" << id << ", " << start.InSecondsF()
           << ", " << end.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);

  DCHECK(!id.empty());
  CHECK(IsValidId_Locked(id));
  DCHECK(start >= base::TimeDelta()) << start.InSecondsF();
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(duration_ != kNoTimestamp);
  DCHECK(start <= duration_) << "start " << start.InSecondsF()
                             << " duration " << duration_.InSecondsF();

  if (start == duration_)
    return;

  source_state_map_[id]->Remove(start, end, duration_);
  host_->OnBufferedTimeRangesChanged(GetBufferedRanges_Locked());
}

bool ChunkDemuxer::CanChangeType(const std::string& id,
                                 const std::string& content_type,
                                 const std::string& codecs) {
  // Note, Chromium currently will not compare content_type and codecs, if any,
  // with previous content_type and codecs of the SourceBuffer.
  // TODO(wolenetz): Consider returning false if the codecs parameters are ever
  // made to be precise such that they signal that the number of tracks of
  // various media types differ from the first initialization segment (if
  // received already).  Switching to an audio-only container, when the first
  // initialization segment only contained non-audio tracks, is one example we
  // could enforce earlier here.

  DVLOG(1) << __func__ << " id=" << id << " content_type=" << content_type
           << " codecs=" << codecs;
  base::AutoLock auto_lock(lock_);

  DCHECK(IsValidId_Locked(id));

  if (!supports_change_type_) {
    return false;
  }

  // CanChangeType() doesn't care if there has or hasn't been received a first
  // initialization segment for the source buffer corresponding to |id|.

  std::unique_ptr<media::StreamParser> stream_parser(
      CreateParserForTypeAndCodecs(content_type, codecs, media_log_));
  return !!stream_parser;
}

void ChunkDemuxer::ChangeType(const std::string& id,
                              const std::string& content_type,
                              const std::string& codecs) {
  DVLOG(1) << __func__ << " id=" << id << " content_type=" << content_type
           << " codecs=" << codecs;

  base::AutoLock auto_lock(lock_);

  DCHECK(state_ == INITIALIZING || state_ == INITIALIZED) << state_;
  DCHECK(IsValidId_Locked(id));

  std::unique_ptr<media::StreamParser> stream_parser(
      CreateParserForTypeAndCodecs(content_type, codecs, media_log_));
  // Caller should query CanChangeType() first to protect from failing this.
  DCHECK(stream_parser);
  source_state_map_[id]->ChangeType(std::move(stream_parser),
                                    ExpectedCodecs(content_type, codecs));
}

double ChunkDemuxer::GetDuration() {
  base::AutoLock auto_lock(lock_);
  return GetDuration_Locked();
}

double ChunkDemuxer::GetDuration_Locked() {
  lock_.AssertAcquired();
  if (duration_ == kNoTimestamp)
    return std::numeric_limits<double>::quiet_NaN();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration_ == kInfiniteDuration)
    return std::numeric_limits<double>::infinity();

  if (user_specified_duration_ >= 0)
    return user_specified_duration_;

  return duration_.InSecondsF();
}

void ChunkDemuxer::SetDuration(double duration) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetDuration(" << duration << ")";
  DCHECK_GE(duration, 0);

  if (duration == GetDuration_Locked())
    return;

  // Compute & bounds check the base::TimeDelta representation of duration.
  // This can be different if the value of |duration| doesn't fit the range or
  // precision of base::TimeDelta.
  base::TimeDelta min_duration = base::TimeDelta::FromInternalValue(1);
  // Don't use base::TimeDelta::Max() here, as we want the largest finite time
  // delta.
  base::TimeDelta max_duration = base::TimeDelta::FromInternalValue(
      std::numeric_limits<int64_t>::max() - 1);
  double min_duration_in_seconds = min_duration.InSecondsF();
  double max_duration_in_seconds = max_duration.InSecondsF();

  base::TimeDelta duration_td;
  if (duration == std::numeric_limits<double>::infinity()) {
    duration_td = media::kInfiniteDuration;
  } else if (duration < min_duration_in_seconds) {
    duration_td = min_duration;
  } else if (duration > max_duration_in_seconds) {
    duration_td = max_duration;
  } else {
    duration_td =
        base::Microseconds(duration * base::Time::kMicrosecondsPerSecond);
  }

  DCHECK(duration_td.is_positive());

  user_specified_duration_ = duration;
  duration_ = duration_td;
  host_->SetDuration(duration_);

  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->OnSetDuration(duration_);
  }
}

bool ChunkDemuxer::IsParsingMediaSegment(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "IsParsingMediaSegment(" << id << ")";
  CHECK(IsValidId_Locked(id));

  return source_state_map_[id]->parsing_media_segment();
}

bool ChunkDemuxer::GetGenerateTimestampsFlag(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "GetGenerateTimestampsFlag(" << id << ")";
  CHECK(IsValidId_Locked(id));

  return source_state_map_[id]->generate_timestamps_flag();
}

void ChunkDemuxer::SetSequenceMode(const std::string& id,
                                   bool sequence_mode) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetSequenceMode(" << id << ", " << sequence_mode << ")";
  CHECK(IsValidId_Locked(id));
  DCHECK_NE(state_, ENDED);

  source_state_map_[id]->SetSequenceMode(sequence_mode);
}

void ChunkDemuxer::SetGroupStartTimestampIfInSequenceMode(
    const std::string& id,
    base::TimeDelta timestamp_offset) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetGroupStartTimestampIfInSequenceMode(" << id << ", "
           << timestamp_offset.InSecondsF() << ")";
  CHECK(IsValidId_Locked(id));
  DCHECK_NE(state_, ENDED);

  source_state_map_[id]->SetGroupStartTimestampIfInSequenceMode(
      timestamp_offset);
}


void ChunkDemuxer::MarkEndOfStream(PipelineStatus status) {
  DVLOG(1) << "MarkEndOfStream(" << status << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, WAITING_FOR_INIT);
  DCHECK_NE(state_, ENDED);

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  if (state_ == INITIALIZING) {
    MEDIA_LOG(ERROR, media_log_)
        << "MediaSource endOfStream before demuxer initialization completes "
           "(before HAVE_METADATA) is treated as an error. This may also occur "
           "as consequence of other MediaSource errors before HAVE_METADATA.";
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  bool old_waiting_for_data = IsSeekWaitingForData_Locked();
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->MarkEndOfStream();
  }

  CompletePendingReadsIfPossible();

  // Give a chance to resume the pending seek process.
  if (status != PIPELINE_OK) {
    DCHECK(status == CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR ||
           status == CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR);
    ReportError_Locked(status);
    return;
  }

  ChangeState_Locked(ENDED);
  DecreaseDurationIfNecessary();

  if (old_waiting_for_data && !IsSeekWaitingForData_Locked() && seek_cb_)
    RunSeekCB_Locked(PIPELINE_OK);
}

void ChunkDemuxer::UnmarkEndOfStream() {
  DVLOG(1) << "UnmarkEndOfStream()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == ENDED || state_ == SHUTDOWN || state_ == PARSE_ERROR)
      << state_;

  // At least ReportError_Locked()'s error reporting to Blink hops threads, so
  // SourceBuffer may not be aware of media element error on another operation
  // that might race to this point.
  if (state_ == PARSE_ERROR || state_ == SHUTDOWN)
    return;

  ChangeState_Locked(INITIALIZED);

  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->UnmarkEndOfStream();
  }
}

void ChunkDemuxer::Shutdown() {
  DVLOG(1) << "Shutdown()";
  base::AutoLock auto_lock(lock_);

  if (state_ == SHUTDOWN)
    return;

  ShutdownAllStreams();

  ChangeState_Locked(SHUTDOWN);

  if (seek_cb_)
    RunSeekCB_Locked(PIPELINE_ERROR_ABORT);
}

void ChunkDemuxer::SetMemoryLimitsForTest(DemuxerStream::Type type,
                                          size_t memory_limit) {
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->SetMemoryLimits(type, memory_limit);
  }
}

void ChunkDemuxer::ChangeState_Locked(State new_state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxer::ChangeState_Locked() : "
           << state_ << " -> " << new_state;

  // TODO(wolenetz): Change to DCHECK once less verification in release build is
  // needed. See https://crbug.com/786975.
  // Disallow changes from at or beyond PARSE_ERROR to below PARSE_ERROR.
  CHECK(!(state_ >= PARSE_ERROR && new_state < PARSE_ERROR));

  state_ = new_state;
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);
}

void ChunkDemuxer::ReportError_Locked(PipelineStatus error) {
  DVLOG(1) << "ReportError_Locked(" << error << ")";
  lock_.AssertAcquired();
  DCHECK(error != PIPELINE_OK);

  ChangeState_Locked(PARSE_ERROR);

  if (init_cb_) {
    RunInitCB_Locked(error);
    return;
  }

  ShutdownAllStreams();
  if (seek_cb_) {
    RunSeekCB_Locked(error);
    return;
  }

  base::AutoUnlock auto_unlock(lock_);
  host_->OnDemuxerError(error);
}

bool ChunkDemuxer::IsSeekWaitingForData_Locked() const {
  lock_.AssertAcquired();
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    if (itr->second->IsSeekWaitingForData())
      return true;
  }

  return false;
}

void ChunkDemuxer::OnSourceInitDone(
    const std::string& source_id,
    const StreamParser::InitParameters& params) {
  DVLOG(1) << "OnSourceInitDone source_id=" << source_id
           << " duration=" << params.duration.InSecondsF();
  lock_.AssertAcquired();

  // TODO(wolenetz): Change these to DCHECKs once less verification in release
  // build is needed. See https://crbug.com/786975.
  CHECK(!pending_source_init_ids_.empty());
  CHECK(IsValidId_Locked(source_id));
  CHECK(pending_source_init_ids_.find(source_id) !=
        pending_source_init_ids_.end());
  CHECK(init_cb_);
  CHECK_EQ(state_, INITIALIZING);
  if (audio_streams_.empty() && video_streams_.empty()) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (!params.duration.is_zero() && duration_ == kNoTimestamp)
    UpdateDuration(params.duration);

  if (!params.timeline_offset.is_null()) {
    if (!timeline_offset_.is_null() &&
        params.timeline_offset != timeline_offset_) {
      MEDIA_LOG(ERROR, media_log_)
          << "Timeline offset is not the same across all SourceBuffers.";
      ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
      return;
    }

    timeline_offset_ = params.timeline_offset;
  }

  if (params.liveness != StreamLiveness::kUnknown) {
    for (const auto& s : audio_streams_)
      s->SetLiveness(params.liveness);
    for (const auto& s : video_streams_)
      s->SetLiveness(params.liveness);
  }

  // Wait until all streams have initialized.
  pending_source_init_ids_.erase(source_id);
  if (!pending_source_init_ids_.empty())
    return;

  SeekAllSources(GetStartTime());
  StartReturningData();

  if (duration_ == kNoTimestamp)
    duration_ = kInfiniteDuration;

  // The demuxer is now initialized after the |start_timestamp_| was set.
  // TODO(wolenetz): Change these to DCHECKs once less verification in release
  // build is needed. See https://crbug.com/786975.
  CHECK_EQ(state_, INITIALIZING);
  ChangeState_Locked(INITIALIZED);
  RunInitCB_Locked(PIPELINE_OK);
}

// static
MediaTrack::Id ChunkDemuxer::GenerateMediaTrackId() {
  static unsigned g_track_count = 0;
  return MediaTrack::Id(base::NumberToString(++g_track_count));
}

ChunkDemuxerStream* ChunkDemuxer::CreateDemuxerStream(
    const std::string& source_id,
    DemuxerStream::Type type) {
  // New ChunkDemuxerStreams can be created only during initialization segment
  // processing, which happens when a new chunk of data is appended and the
  // lock_ must be held by ChunkDemuxer::RunSegmentParserLoop/AppendChunks.
  lock_.AssertAcquired();

  MediaTrack::Id media_track_id = GenerateMediaTrackId();

  OwnedChunkDemuxerStreamVector* owning_vector = nullptr;
  switch (type) {
    case DemuxerStream::AUDIO:
      owning_vector = &audio_streams_;
      break;

    case DemuxerStream::VIDEO:
      owning_vector = &video_streams_;
      break;

    case DemuxerStream::UNKNOWN:
      NOTREACHED();
  }

  std::unique_ptr<ChunkDemuxerStream> stream =
      std::make_unique<ChunkDemuxerStream>(type, media_track_id);
  DCHECK(track_id_to_demux_stream_map_.find(media_track_id) ==
         track_id_to_demux_stream_map_.end());
  track_id_to_demux_stream_map_[media_track_id] = stream.get();
  id_to_streams_map_[source_id].push_back(stream.get());
  stream->SetEnabled(owning_vector->empty(), base::TimeDelta());
  owning_vector->push_back(std::move(stream));
  return owning_vector->back().get();
}

bool ChunkDemuxer::IsValidId_Locked(const std::string& source_id) const {
  lock_.AssertAcquired();
  return source_state_map_.count(source_id) > 0u;
}

void ChunkDemuxer::UpdateDuration(base::TimeDelta new_duration) {
  DCHECK(duration_ != new_duration ||
         user_specified_duration_ != new_duration.InSecondsF());
  user_specified_duration_ = -1;
  duration_ = new_duration;
  host_->SetDuration(new_duration);
}

void ChunkDemuxer::IncreaseDurationIfNecessary(base::TimeDelta new_duration) {
  DCHECK(new_duration != kNoTimestamp);
  DCHECK(new_duration != kInfiniteDuration);

  // Per April 1, 2014 MSE spec editor's draft:
  // https://dvcs.w3.org/hg/html-media/raw-file/d471a4412040/media-source/
  //     media-source.html#sourcebuffer-coded-frame-processing
  // 5. If the media segment contains data beyond the current duration, then run
  //    the duration change algorithm with new duration set to the maximum of
  //    the current duration and the group end timestamp.

  if (new_duration <= duration_)
    return;

  DVLOG(2) << __func__ << ": Increasing duration: " << duration_.InSecondsF()
           << " -> " << new_duration.InSecondsF();

  UpdateDuration(new_duration);
}

void ChunkDemuxer::DecreaseDurationIfNecessary() {
  lock_.AssertAcquired();

  base::TimeDelta max_duration;

  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    max_duration = std::max(max_duration,
                            itr->second->GetMaxBufferedDuration());
  }

  if (max_duration.is_zero())
    return;

  // Note: be careful to also check |user_specified_duration_|, which may have
  // higher precision than |duration_|.
  if (max_duration < duration_ ||
      max_duration.InSecondsF() < user_specified_duration_) {
    UpdateDuration(max_duration);
  }
}

Ranges<base::TimeDelta> ChunkDemuxer::GetBufferedRanges() const {
  base::AutoLock auto_lock(lock_);
  return GetBufferedRanges_Locked();
}

Ranges<base::TimeDelta> ChunkDemuxer::GetBufferedRanges_Locked() const {
  lock_.AssertAcquired();

  bool ended = state_ == ENDED;
  // TODO(acolwell): When we start allowing SourceBuffers that are not active,
  // we'll need to update this loop to only add ranges from active sources.
  SourceBufferState::RangesList ranges_list;
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    ranges_list.push_back(itr->second->GetBufferedRanges(duration_, ended));
  }

  return SourceBufferState::ComputeRangesIntersection(ranges_list, ended);
}

void ChunkDemuxer::StartReturningData() {
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->StartReturningData();
  }
}

void ChunkDemuxer::AbortPendingReads_Locked() {
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->AbortReads();
  }
}

void ChunkDemuxer::SeekAllSources(base::TimeDelta seek_time) {
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->Seek(seek_time);
  }
}

void ChunkDemuxer::CompletePendingReadsIfPossible() {
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->CompletePendingReadIfPossible();
  }
}

void ChunkDemuxer::ShutdownAllStreams() {
  for (auto itr = source_state_map_.begin(); itr != source_state_map_.end();
       ++itr) {
    itr->second->Shutdown();
  }
}

void ChunkDemuxer::RunInitCB_Locked(PipelineStatus status) {
  lock_.AssertAcquired();
  DCHECK(init_cb_);
  TRACE_EVENT_ASYNC_END1("media", "ChunkDemuxer::Initialize", this, "status",
                         PipelineStatusToString(status));
  std::move(init_cb_).Run(status);
}

void ChunkDemuxer::RunSeekCB_Locked(PipelineStatus status) {
  lock_.AssertAcquired();
  DCHECK(seek_cb_);
  TRACE_EVENT_ASYNC_END1("media", "ChunkDemuxer::Seek", this, "status",
                         PipelineStatusToString(status));
  std::move(seek_cb_).Run(status);
}

}  // namespace media
