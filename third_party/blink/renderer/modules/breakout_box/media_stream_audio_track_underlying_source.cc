// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_source.h"

#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_buffer.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

// Expects all calls to SetFormat() and CopyIntoAudioBuffer() to come from the
// same thread/sequence. This is almost certainly the realtime Audio capture
// thread, from a microphone accessed through getUserMedia(). As such, this
// class is designed to minimize memory allocations.
// This class may be created on a different thread/sequence than it is used.
class AudioBufferPoolImpl
    : public MediaStreamAudioTrackUnderlyingSource::AudioBufferPool {
 public:
  AudioBufferPoolImpl() = default;
  ~AudioBufferPoolImpl() override = default;

  AudioBufferPoolImpl(const AudioBufferPoolImpl&) = delete;
  AudioBufferPoolImpl& operator=(const AudioBufferPoolImpl&) = delete;

  void SetFormat(const media::AudioParameters params) override {
    CHECK(params.IsValid());

    if (!params_.Equals(params)) {
      buffers_.clear();
    }

    params_ = params;
  }

  // Copies `audio_bus` into a media::AudioBuffer. Allocates a new AudioBuffer
  // if none are available.
  scoped_refptr<media::AudioBuffer> CopyIntoAudioBuffer(
      const media::AudioBus& audio_bus,
      base::TimeTicks capture_time) override {
    // SetFormat() should have been called once already.
    CHECK(params_.IsValid());
    CHECK_EQ(params_.channels(), audio_bus.channels());
    CHECK_EQ(params_.frames_per_buffer(), audio_bus.frames());

    auto buffer = TakeUnusedBuffer();

    if (!buffer) {
      return AllocateAndSaveNewBuffer(audio_bus, capture_time);
    }

    // We should not be modifying the channel data of a buffer currently
    // in use.
    CHECK(buffer->HasOneRef());
    CHECK_EQ(buffer->channel_count(), audio_bus.channels());
    CHECK_EQ(buffer->frame_count(), audio_bus.frames());

    buffer->set_timestamp(capture_time - base::TimeTicks());

    // Copy the data over.
    const std::vector<uint8_t*>& dest_data = buffer->channel_data();
    for (int ch = 0; ch < audio_bus.channels(); ++ch) {
      const float* src_channel = audio_bus.channel(ch);
      memcpy(dest_data[ch], src_channel, sizeof(float) * audio_bus.frames());
    }

    buffers_.push_back(buffer);
    return buffer;
  }

  int GetSizeForTesting() override { return buffers_.size(); }

 private:
  scoped_refptr<media::AudioBuffer> AllocateAndSaveNewBuffer(
      const media::AudioBus& audio_bus,
      base::TimeTicks capture_time) {
    auto buffer = media::AudioBuffer::CopyFrom(params_.sample_rate(),
                                               capture_time - base::TimeTicks(),
                                               &audio_bus, nullptr);
    buffers_.push_back(buffer);
    return buffer;
  }

  // Returns the LRU unused buffer, or nullptr if there are no unused buffers.
  // A buffer is "unused" if `buffers_` is its only reference: such a buffer
  // could not still be used by clients, and can be recycled.
  scoped_refptr<media::AudioBuffer> TakeUnusedBuffer() {
    if (!buffers_.size()) {
      return nullptr;
    }

    // Return the LRU buffer if it's not currently used.
    // A simple local test shows that a single buffer is often all that is
    // needed.
    if (buffers_.front()->HasOneRef()) {
      return buffers_.TakeFirst();
    }

    // Search any other unused buffer in our queue.
    for (auto it = buffers_.begin(); it != buffers_.end(); ++it) {
      if ((*it)->HasOneRef()) {
        auto buffer = *it;
        buffers_.erase(it);
        return buffer;
      }
    }

    // We will need to allocate a new buffer.
    return nullptr;
  }

  media::AudioParameters params_;

  static constexpr int kInlineCapacity = 4;
  WTF::Deque<scoped_refptr<media::AudioBuffer>, kInlineCapacity> buffers_;
};

MediaStreamAudioTrackUnderlyingSource::MediaStreamAudioTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    ScriptWrappable* media_stream_track_processor,
    wtf_size_t max_queue_size)
    : AudioDataQueueUnderlyingSource(script_state, max_queue_size),
      media_stream_track_processor_(media_stream_track_processor),
      track_(track),
      buffer_pool_(std::make_unique<AudioBufferPoolImpl>()) {
  DCHECK(track_);
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kReadableAudio);
}

bool MediaStreamAudioTrackUnderlyingSource::StartFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaStreamAudioTrack* audio_track = MediaStreamAudioTrack::From(track_);
  if (!audio_track) {
    return false;
  }

  if (is_connected_to_track_) {
    return true;
  }

  WebMediaStreamAudioSink::AddToAudioTrack(this, WebMediaStreamTrack(track_));
  is_connected_to_track_ = this;
  return true;
}

void MediaStreamAudioTrackUnderlyingSource::DisconnectFromTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!track_) {
    return;
  }

  WebMediaStreamAudioSink::RemoveFromAudioTrack(this,
                                                WebMediaStreamTrack(track_));
  is_connected_to_track_.Clear();
  track_.Clear();
}

void MediaStreamAudioTrackUnderlyingSource::ContextDestroyed() {
  AudioDataQueueUnderlyingSource::ContextDestroyed();
  DisconnectFromTrack();
}

void MediaStreamAudioTrackUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(media_stream_track_processor_);
  visitor->Trace(track_);
  AudioDataQueueUnderlyingSource::Trace(visitor);
}

void MediaStreamAudioTrackUnderlyingSource::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  QueueFrame(
      buffer_pool_->CopyIntoAudioBuffer(audio_bus, estimated_capture_time));
}

void MediaStreamAudioTrackUnderlyingSource::StopFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisconnectFromTrack();
}

void MediaStreamAudioTrackUnderlyingSource::OnSetFormat(
    const media::AudioParameters& params) {
  buffer_pool_->SetFormat(params);
}

std::unique_ptr<ReadableStreamTransferringOptimizer>
MediaStreamAudioTrackUnderlyingSource::GetTransferringOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<AudioDataQueueTransferOptimizer>(
      this, GetRealmRunner(), MaxQueueSize(),
      CrossThreadBindOnce(
          &MediaStreamAudioTrackUnderlyingSource::OnSourceTransferStarted,
          WrapCrossThreadWeakPersistent(this)),
      CrossThreadBindOnce(
          &MediaStreamAudioTrackUnderlyingSource::ClearTransferredSource,
          WrapCrossThreadWeakPersistent(this)));
}

void MediaStreamAudioTrackUnderlyingSource::OnSourceTransferStarted(
    scoped_refptr<base::SequencedTaskRunner> transferred_runner,
    CrossThreadPersistent<TransferredAudioDataQueueUnderlyingSource> source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransferSource(std::move(source));
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kReadableAudioWorker);
}

MediaStreamAudioTrackUnderlyingSource::AudioBufferPool*
MediaStreamAudioTrackUnderlyingSource::GetAudioBufferPoolForTesting() {
  return buffer_pool_.get();
}

}  // namespace blink
