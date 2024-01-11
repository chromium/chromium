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

namespace blink {

MediaStreamAudioTrackUnderlyingSource::MediaStreamAudioTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    ScriptWrappable* media_stream_track_processor,
    wtf_size_t max_queue_size)
    : AudioDataQueueUnderlyingSource(script_state, max_queue_size),
      media_stream_track_processor_(media_stream_track_processor),
      track_(track),
      buffer_pool_(base::MakeRefCounted<media::AudioBufferMemoryPool>()) {
  DCHECK(track_);
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kReadableAudio);
}

bool MediaStreamAudioTrackUnderlyingSource::StartFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaStreamAudioTrack* audio_track = MediaStreamAudioTrack::From(track_);
  if (!audio_track)
    return false;

  if (is_connected_to_track_) {
    return true;
  }

  WebMediaStreamAudioSink::AddToAudioTrack(this, WebMediaStreamTrack(track_));
  is_connected_to_track_ = this;
  return true;
}

void MediaStreamAudioTrackUnderlyingSource::DisconnectFromTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!track_)
    return;

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
  DCHECK(audio_parameters_.IsValid());

  auto data_copy = media::AudioBuffer::CopyFrom(
      audio_parameters_.sample_rate(),
      estimated_capture_time - base::TimeTicks(), &audio_bus, buffer_pool_);

  QueueFrame(std::move(data_copy));
}

void MediaStreamAudioTrackUnderlyingSource::StopFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisconnectFromTrack();
}

void MediaStreamAudioTrackUnderlyingSource::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(params.IsValid());
  audio_parameters_ = params;
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

}  // namespace blink
