// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
PushableMediaStreamAudioSource::LivenessBroker::LivenessBroker(
    PushableMediaStreamAudioSource* source)
    : source_(source) {}

void PushableMediaStreamAudioSource::LivenessBroker::
    OnSourceDestroyedOrStopped() {
  WTF::MutexLocker locker(mutex_);
  source_ = nullptr;
}

void PushableMediaStreamAudioSource::LivenessBroker::PushAudioData(
    scoped_refptr<media::AudioBuffer> data) {
  WTF::MutexLocker locker(mutex_);
  if (!source_)
    return;

  source_->DeliverData(std::move(data));
}

PushableMediaStreamAudioSource::PushableMediaStreamAudioSource(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SequencedTaskRunner> audio_task_runner)
    : MediaStreamAudioSource(std::move(main_task_runner), /* is_local */ true),
      audio_task_runner_(std::move(audio_task_runner)),
      liveness_broker_(
          base::MakeRefCounted<PushableMediaStreamAudioSource::LivenessBroker>(
              this)) {}

PushableMediaStreamAudioSource::~PushableMediaStreamAudioSource() {
  liveness_broker_->OnSourceDestroyedOrStopped();
}

void PushableMediaStreamAudioSource::PushAudioData(
    scoped_refptr<media::AudioBuffer> data) {
  DCHECK(data);

  if (audio_task_runner_->RunsTasksInCurrentSequence()) {
    DeliverData(std::move(data));
    return;
  }

  PostCrossThreadTask(
      *audio_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &PushableMediaStreamAudioSource::LivenessBroker::PushAudioData,
          liveness_broker_, std::move(data)));
}

void PushableMediaStreamAudioSource::DeliverData(
    scoped_refptr<media::AudioBuffer> data) {
  DCHECK(audio_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(data);

  const int sample_rate = data->sample_rate();
  const int frame_count = data->frame_count();
  const int channel_count = data->channel_count();

  media::AudioParameters params = GetAudioParameters();
  if (!params.IsValid() ||
      params.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY ||
      last_channels_ != channel_count || last_sample_rate_ != sample_rate ||
      last_frames_ != frame_count) {
    SetFormat(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::GuessChannelLayout(channel_count), sample_rate, frame_count));
    last_channels_ = channel_count;
    last_sample_rate_ = sample_rate;
    last_frames_ = frame_count;
  }

  // If |data|'s sample format has the same memory layout as a media::AudioBus,
  // |audio_bus| will simply wrap it. Otherwise, |data| will be copied and
  // converted into |audio_bus|.
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBuffer::WrapOrCopyToAudioBus(data);

  DeliverDataToTracks(*audio_bus, base::TimeTicks() + data->timestamp());
}

bool PushableMediaStreamAudioSource::EnsureSourceIsStarted() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  is_running_ = true;
  return true;
}

void PushableMediaStreamAudioSource::EnsureSourceIsStopped() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  liveness_broker_->OnSourceDestroyedOrStopped();
  is_running_ = false;
}

}  // namespace blink
