// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SYMPHONIA_DEMUXER_STREAM_H_
#define MEDIA_FILTERS_SYMPHONIA_DEMUXER_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer_queue.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_track.h"
#include "media/base/timestamp_constants.h"

namespace media {

class DecoderBuffer;

// A `DemuxerStream` for a single track demuxed by `SymphoniaDemuxer`.
//
// The stream holds a bounded queue of demuxed packets. Whenever it has spare
// capacity, or a `Read()` cannot be satisfied from the queue, it runs the
// `request_data_cb` supplied at construction so that the owning demuxer reads
// more packets from the container.
//
// Currently only audio tracks are supported.
// TODO(crbug.com/550619039): Add video support, likely by splitting this into a
// common base class with audio- and video-specific subclasses.
//
// All methods must be called on the media sequence.
class MEDIA_EXPORT SymphoniaDemuxerStream : public DemuxerStream {
 public:
  // `request_data_cb` is run (synchronously) whenever the stream wants the
  // demuxer to read more packets.
  SymphoniaDemuxerStream(uint32_t track_id,
                         MediaTrack::Id media_track_id,
                         AudioDecoderConfig audio_config,
                         base::TimeDelta duration,
                         base::RepeatingClosure request_data_cb);

  SymphoniaDemuxerStream(const SymphoniaDemuxerStream&) = delete;
  SymphoniaDemuxerStream& operator=(const SymphoniaDemuxerStream&) = delete;

  ~SymphoniaDemuxerStream() override;

  // DemuxerStream implementation.
  Type type() const override;
  StreamLiveness liveness() const override;
  void Read(uint32_t count, ReadCB read_cb) override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;

  // The Symphonia track ID this stream carries packets for.
  uint32_t track_id() const { return track_id_; }
  // The ID of the `MediaTrack` for this stream.
  const MediaTrack::Id& media_track_id() const { return media_track_id_; }
  base::TimeDelta duration() const { return duration_; }

  bool is_enabled() const { return is_enabled_; }

  // Returns the duration estimated from the packets enqueued so far, or
  // `kNoTimestamp` if none have been. Like `FFmpegDemuxerStream::duration()`,
  // this is the latest packet's start time, ignoring packets that are entirely
  // discard padding.
  base::TimeDelta estimated_duration() const { return estimated_duration_; }

  // Sets the value returned by `liveness()`. Defaults to `kRecorded`.
  void set_liveness(StreamLiveness liveness) { liveness_ = liveness; }

  // Sets whether `SupportsConfigChanges()` returns true. Must be called before
  // the stream is handed to the pipeline.
  void set_supports_config_changes(bool supports_config_changes) {
    supports_config_changes_ = supports_config_changes;
  }

  // Queues a config transition for packets enqueued from now on, e.g. when a
  // chained Ogg stream starts. The first read after all previously queued
  // packets returns `kConfigChanged` so the decoder resets for the new stream,
  // even when `audio_config` matches the previous config. Requires
  // `SupportsConfigChanges()`.
  void ChangeConfig(AudioDecoderConfig audio_config);

  // Enables or disables the stream. Disabling drops all queued packets and
  // satisfies any pending read with an end-of-stream buffer. Packets enqueued
  // while disabled are dropped. End-of-stream and aborted state are kept.
  void SetEnabled(bool enabled);

  // Appends a demuxed packet to the queue and satisfies any pending read.
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);

  // Marks the end of the stream; once the queue drains, reads return an
  // end-of-stream buffer.
  void SetEndOfStream();

  // Drops all queued packets and clears end-of-stream and aborted state, e.g.
  // in preparation for a seek.
  void FlushBuffers();

  // Permanently stops the stream; any pending and future reads are aborted.
  void Stop();

  // Aborts any pending read and drops all queued packets. Future reads are
  // aborted until the next `FlushBuffers()`.
  void Abort();

  // Returns true if the stream wants more packets. That is never the case when
  // it is stopped, aborted, or disabled, since `EnqueueBuffer()` would drop
  // them. Otherwise, either its queue is below capacity and it has not reached
  // end-of-stream, or it has a pending read.
  bool HasAvailableCapacity() const;
  bool has_pending_read() const { return !!read_cb_; }

  // Returns the number of bytes of queued packet data.
  int64_t GetMemoryUsage() const;

 private:
  struct PendingConfigSegment {
    explicit PendingConfigSegment(AudioDecoderConfig config);
    ~PendingConfigSegment();

    AudioDecoderConfig config;
    DecoderBufferQueue buffers;
  };

  // Satisfies the pending read, if any, from the queue or with an end-of-stream
  // buffer, then requests more data if the queue has spare capacity.
  void SatisfyPendingRead();

  // Satisfies the pending read with a single end-of-stream buffer.
  void SatisfyPendingReadWithEndOfStream();

  // Updates `estimated_duration_` for a newly enqueued `buffer`.
  void UpdateEstimatedDuration(const DecoderBuffer& buffer);

  // Drops all queued packets while keeping the latest pending config change
  // (if any) so the next read still emits `kConfigChanged`.
  void DropQueuedPackets();

  // Total number of queued packets across `buffer_queue_` and
  // `pending_segments_`.
  size_t TotalQueuedPackets() const;

  const uint32_t track_id_;
  const MediaTrack::Id media_track_id_;
  AudioDecoderConfig audio_config_;
  const base::TimeDelta duration_;
  const base::RepeatingClosure request_data_cb_;

  ReadCB read_cb_;
  uint32_t requested_count_ = 0;
  DecoderBufferQueue buffer_queue_;

  // Chained stream segments queued by `ChangeConfig()`. Each segment holds its
  // config and packets until earlier queues drain, then a read returns
  // `kConfigChanged` and promotes the front segment into `buffer_queue_`.
  base::circular_deque<std::unique_ptr<PendingConfigSegment>> pending_segments_;

  bool end_of_stream_ = false;
  bool is_enabled_ = true;
  bool stopped_ = false;
  bool aborted_ = false;
  StreamLiveness liveness_ = StreamLiveness::kRecorded;
  base::TimeDelta estimated_duration_ = kNoTimestamp;
  bool supports_config_changes_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SYMPHONIA_DEMUXER_STREAM_H_
