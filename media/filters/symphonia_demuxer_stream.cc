// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer_stream.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "media/base/decoder_buffer.h"

namespace media {

namespace {

// Maximum number of demuxed packets to queue per stream before the stream
// stops requesting more data. Demuxing happens on a blocking sequence, so a
// small queue lets reads be served without waiting on I/O while keeping memory
// bounded. For common compressed audio codecs this is roughly one to a few
// seconds of audio (e.g. ~1.3s of MP3 at 1152 samples/frame @ 44.1kHz, ~1.2s of
// AAC at 1024 samples/frame @ 44.1kHz).
constexpr size_t kMaxQueuedPackets = 50;

}  // namespace

SymphoniaDemuxerStream::PendingConfigSegment::PendingConfigSegment(
    AudioDecoderConfig config)
    : config(std::move(config)) {}

SymphoniaDemuxerStream::PendingConfigSegment::~PendingConfigSegment() = default;

SymphoniaDemuxerStream::SymphoniaDemuxerStream(
    uint32_t track_id,
    MediaTrack::Id media_track_id,
    AudioDecoderConfig audio_config,
    base::TimeDelta duration,
    base::RepeatingClosure request_data_cb)
    : track_id_(track_id),
      media_track_id_(std::move(media_track_id)),
      audio_config_(std::move(audio_config)),
      duration_(duration),
      request_data_cb_(std::move(request_data_cb)) {
  CHECK(request_data_cb_);
}

SymphoniaDemuxerStream::~SymphoniaDemuxerStream() = default;

DemuxerStream::Type SymphoniaDemuxerStream::type() const {
  return AUDIO;
}

StreamLiveness SymphoniaDemuxerStream::liveness() const {
  return liveness_;
}

void SymphoniaDemuxerStream::Read(uint32_t count, ReadCB read_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!read_cb_) << "Overlapping reads not supported";
  read_cb_ = base::BindPostTaskToCurrentDefault(std::move(read_cb));
  requested_count_ = count;

  if (stopped_ || aborted_) {
    std::move(read_cb_).Run(kAborted, {});
    return;
  }

  if (!is_enabled_) {
    SatisfyPendingReadWithEndOfStream();
    return;
  }

  SatisfyPendingRead();
}

void SymphoniaDemuxerStream::EnableBitstreamConverter() {
  NOTIMPLEMENTED();
}

bool SymphoniaDemuxerStream::SupportsConfigChanges() {
  return supports_config_changes_;
}

AudioDecoderConfig SymphoniaDemuxerStream::audio_decoder_config() {
  return audio_config_;
}

VideoDecoderConfig SymphoniaDemuxerStream::video_decoder_config() {
  // `type()` is always `AUDIO`, so callers must not ask for a video config.
  // TODO(crbug.com/550619039): Return the video config once video tracks are
  // supported.
  NOTREACHED();
}

void SymphoniaDemuxerStream::ChangeConfig(AudioDecoderConfig audio_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(supports_config_changes_);

  // Always emit `kConfigChanged` at a chained stream boundary, even when the
  // new stream's config matches the previous one, so the audio decoder resets
  // its overlap/pre-skip state. If a trailing segment never received any
  // packets, replace it instead of emitting two empty resets in a row.
  if (!pending_segments_.empty() &&
      pending_segments_.back()->buffers.IsEmpty()) {
    pending_segments_.back()->config = std::move(audio_config);
  } else {
    pending_segments_.push_back(
        std::make_unique<PendingConfigSegment>(std::move(audio_config)));
  }
  SatisfyPendingRead();
}

void SymphoniaDemuxerStream::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_enabled_ == enabled) {
    return;
  }
  is_enabled_ = enabled;
  if (is_enabled_) {
    return;
  }

  // Only drop queued packets. Unlike `FlushBuffers()`, keep the end-of-stream
  // and aborted state: the demuxer does not read past end of stream, so a
  // re-enabled stream that lost it would wait forever, and an aborted stream
  // must keep aborting reads until the next seek.
  DropQueuedPackets();
  if (read_cb_) {
    SatisfyPendingReadWithEndOfStream();
  }
}

void SymphoniaDemuxerStream::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Symphonia demuxes packets for every track in the container, so packets for
  // disabled tracks are simply dropped.
  if (stopped_ || aborted_ || !is_enabled_) {
    return;
  }
  UpdateEstimatedDuration(*buffer);
  // Packets for a pending segment are held back until earlier segments drain.
  if (!pending_segments_.empty()) {
    pending_segments_.back()->buffers.Push(std::move(buffer));
  } else {
    buffer_queue_.Push(std::move(buffer));
  }
  SatisfyPendingRead();
}

void SymphoniaDemuxerStream::UpdateEstimatedDuration(
    const DecoderBuffer& buffer) {
  // Unlike FFmpeg's `AVPacket`, Symphonia's packet timestamps already start at
  // `-delay` for leading trim, and `buffer.duration()` excludes `trim_start`
  // and `trim_end` (leaving zero duration on all-padding packets).
  const auto [start_padding, end_padding] =
      buffer.discard_padding().value_or(DecoderBuffer::DiscardPadding());
  const bool has_padding =
      start_padding.is_positive() || end_padding.is_positive();
  const bool is_padding = has_padding && buffer.duration().is_zero();
  if (!is_padding && buffer.timestamp() > estimated_duration_) {
    estimated_duration_ = buffer.timestamp();
  }
}

void SymphoniaDemuxerStream::SetEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  end_of_stream_ = true;
  SatisfyPendingRead();
}

void SymphoniaDemuxerStream::FlushBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DropQueuedPackets();
  end_of_stream_ = false;
  aborted_ = false;
}

void SymphoniaDemuxerStream::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stopped_ = true;
  Abort();
}

void SymphoniaDemuxerStream::Abort() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  aborted_ = true;
  DropQueuedPackets();
  if (read_cb_) {
    std::move(read_cb_).Run(kAborted, {});
  }
}

bool SymphoniaDemuxerStream::HasAvailableCapacity() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Match `EnqueueBuffer()`: don't ask for packets that would be dropped.
  if (stopped_ || aborted_ || !is_enabled_) {
    return false;
  }
  return (TotalQueuedPackets() < kMaxQueuedPackets && !end_of_stream_) ||
         has_pending_read();
}

int64_t SymphoniaDemuxerStream::GetMemoryUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t total = buffer_queue_.memory_usage_in_bytes();
  for (const auto& segment : pending_segments_) {
    total += segment->buffers.memory_usage_in_bytes();
  }
  return static_cast<int64_t>(total);
}

void SymphoniaDemuxerStream::SatisfyPendingRead() {
  if (!read_cb_) {
    return;
  }

  // All packets for the current segment have been read, so switch to the next
  // queued segment before returning any of its packets.
  if (buffer_queue_.IsEmpty() && !pending_segments_.empty()) {
    std::unique_ptr<PendingConfigSegment> next =
        std::move(pending_segments_.front());
    pending_segments_.pop_front();
    audio_config_ = std::move(next->config);
    while (!next->buffers.IsEmpty()) {
      buffer_queue_.Push(next->buffers.Pop());
    }
    std::move(read_cb_).Run(kConfigChanged, {});
    return;
  }

  if (!buffer_queue_.IsEmpty()) {
    DecoderBufferVector buffers;
    buffers.reserve(
        std::min<size_t>(buffer_queue_.queue_size(), requested_count_));
    while (!buffer_queue_.IsEmpty() && buffers.size() < requested_count_) {
      buffers.emplace_back(buffer_queue_.Pop());
    }
    std::move(read_cb_).Run(kOk, std::move(buffers));
  } else if (end_of_stream_) {
    // The demuxer has nothing more to read after end of stream.
    SatisfyPendingReadWithEndOfStream();
    return;
  }

  if (HasAvailableCapacity()) {
    request_data_cb_.Run();
  }
}

void SymphoniaDemuxerStream::SatisfyPendingReadWithEndOfStream() {
  CHECK(read_cb_);
  DecoderBufferVector buffers;
  buffers.push_back(DecoderBuffer::CreateEOSBuffer());
  std::move(read_cb_).Run(kOk, std::move(buffers));
}

void SymphoniaDemuxerStream::DropQueuedPackets() {
  buffer_queue_.Clear();
  if (!pending_segments_.empty()) {
    std::unique_ptr<PendingConfigSegment> latest =
        std::move(pending_segments_.back());
    latest->buffers.Clear();
    pending_segments_.clear();
    pending_segments_.push_back(std::move(latest));
  }
}

size_t SymphoniaDemuxerStream::TotalQueuedPackets() const {
  size_t total = buffer_queue_.queue_size();
  for (const auto& segment : pending_segments_) {
    total += segment->buffers.queue_size();
  }
  return total;
}

}  // namespace media
