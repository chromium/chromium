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

SymphoniaDemuxerStream::SymphoniaDemuxerStream(
    uint32_t track_id,
    AudioDecoderConfig audio_config,
    base::TimeDelta duration,
    base::RepeatingClosure request_data_cb)
    : track_id_(track_id),
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
  return StreamLiveness::kRecorded;
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
  return false;
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
  buffer_queue_.Clear();
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
  buffer_queue_.Push(std::move(buffer));
  SatisfyPendingRead();
}

void SymphoniaDemuxerStream::SetEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  end_of_stream_ = true;
  SatisfyPendingRead();
}

void SymphoniaDemuxerStream::FlushBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  buffer_queue_.Clear();
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
  buffer_queue_.Clear();
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
  return (buffer_queue_.queue_size() < kMaxQueuedPackets && !end_of_stream_) ||
         has_pending_read();
}

int64_t SymphoniaDemuxerStream::GetMemoryUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<int64_t>(buffer_queue_.memory_usage_in_bytes());
}

void SymphoniaDemuxerStream::SatisfyPendingRead() {
  if (!read_cb_) {
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

}  // namespace media
