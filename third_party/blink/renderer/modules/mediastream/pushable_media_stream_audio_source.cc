// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
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
    std::unique_ptr<AudioFrameSerializationData> data) {
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
    std::unique_ptr<AudioFrameSerializationData> data) {
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
    std::unique_ptr<AudioFrameSerializationData> data) {
  DCHECK(audio_task_runner_->RunsTasksInCurrentSequence());

  const media::AudioBus& audio_bus = *data->data();
  int sample_rate = data->sample_rate();

  media::AudioParameters params = GetAudioParameters();
  if (!params.IsValid() ||
      params.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY ||
      last_channels_ != audio_bus.channels() ||
      last_sample_rate_ != sample_rate || last_frames_ != audio_bus.frames()) {
    SetFormat(
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::GuessChannelLayout(audio_bus.channels()),
                               sample_rate, audio_bus.frames()));
    last_channels_ = audio_bus.channels();
    last_sample_rate_ = sample_rate;
    last_frames_ = audio_bus.frames();
  }

  DeliverDataToTracks(audio_bus, base::TimeTicks() + data->timestamp());
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
