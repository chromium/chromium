// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/channel_layout.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "services/audio/public/cpp/output_device.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace audio {

namespace {

// Volume percent.
const double kOutputVolumePercent = 0.8;

// The number of frames each Render() call will request.
const int kDefaultFrameCount = 1024;

// Keep alive timeout for audio stream.
const int kKeepAliveMs = 1500;

AudioStreamHandler::TestObserver* g_observer_for_testing = NULL;

}  // namespace

class AudioStreamHandler::AudioStreamContainer
    : public media::AudioRendererSink::RenderCallback {
 public:
  explicit AudioStreamContainer(
      std::unique_ptr<service_manager::Connector> connector,
      std::unique_ptr<media::WavAudioHandler> wav_audio)
      : started_(false),
        connector_(std::move(connector)),
        cursor_(0),
        delayed_stop_posted_(false),
        wav_audio_(std::move(wav_audio)) {
    DCHECK(wav_audio_);
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
  }

  ~AudioStreamContainer() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  void Play() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    // Create OutputDevice if it is the first time playing.
    if (device_ == nullptr) {
      const media::AudioParameters params(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::GuessChannelLayout(wav_audio_->num_channels()),
          wav_audio_->sample_rate(), kDefaultFrameCount);
      if (g_observer_for_testing) {
        g_observer_for_testing->Initialize(this, params);
      } else {
        mojo::PendingRemote<audio::mojom::StreamFactory> stream_factory;
        connector_->Connect(audio::mojom::kServiceName,
                            stream_factory.InitWithNewPipeAndPassReceiver());
        device_ = std::make_unique<audio::OutputDevice>(
            std::move(stream_factory), params, this, std::string());
      }
    }

    {
      base::AutoLock al(state_lock_);

      delayed_stop_posted_ = false;
      stop_closure_.Reset(base::BindRepeating(&AudioStreamContainer::StopStream,
                                              base::Unretained(this)));

      if (started_) {
        if (wav_audio_->AtEnd(cursor_))
          cursor_ = 0;
        return;
      } else {
        if (!g_observer_for_testing)
          device_->SetVolume(kOutputVolumePercent);
      }

      cursor_ = 0;
    }

    started_ = true;
    if (g_observer_for_testing)
      g_observer_for_testing->OnPlay();
    else
      device_->Play();
  }

  void Stop() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    StopStream();
    stop_closure_.Cancel();
  }

 private:
  // media::AudioRendererSink::RenderCallback overrides:
  // Following methods could be called from *ANY* thread.
  int Render(base::TimeDelta /* delay */,
             base::TimeTicks /* delay_timestamp */,
             int /* prior_frames_skipped */,
             media::AudioBus* dest) override {
    base::AutoLock al(state_lock_);
    size_t bytes_written = 0;
    if (wav_audio_->AtEnd(cursor_) ||
        !wav_audio_->CopyTo(dest, cursor_, &bytes_written)) {
      if (delayed_stop_posted_)
        return 0;
      delayed_stop_posted_ = true;
      task_runner_->PostDelayedTask(
          FROM_HERE, stop_closure_.callback(),
          base::TimeDelta::FromMilliseconds(kKeepAliveMs));
      return 0;
    }
    cursor_ += bytes_written;
    return dest->frames();
  }

  void OnRenderError() override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioStreamContainer::Stop, base::Unretained(this)));
  }

  void StopStream() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (started_) {
      // Do not hold the |state_lock_| while stopping the output stream.
      if (g_observer_for_testing)
        g_observer_for_testing->OnStop(cursor_);
      else
        device_->Pause();
    }

    started_ = false;
  }

  bool started_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<audio::OutputDevice> device_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // All variables below must be accessed under |state_lock_| when |started_|.
  base::Lock state_lock_;
  size_t cursor_;
  bool delayed_stop_posted_;
  std::unique_ptr<media::WavAudioHandler> wav_audio_;
  base::CancelableClosure stop_closure_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamContainer);
};

AudioStreamHandler::AudioStreamHandler(
    std::unique_ptr<service_manager::Connector> connector,
    const base::StringPiece& wav_data) {
  task_runner_ = base::SequencedTaskRunnerHandle::Get();
  std::unique_ptr<media::WavAudioHandler> wav_audio =
      media::WavAudioHandler::Create(wav_data);
  if (!wav_audio) {
    LOG(ERROR) << "wav_data is not valid";
    return;
  }

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::GuessChannelLayout(wav_audio->num_channels()),
      wav_audio->sample_rate(), kDefaultFrameCount);
  if (!params.IsValid()) {
    LOG(ERROR) << "Audio params are invalid.";
    return;
  }

  // Store the duration of the WAV data then pass the handler to |stream_|.
  duration_ = wav_audio->GetDuration();
  stream_.reset(
      new AudioStreamContainer(std::move(connector), std::move(wav_audio)));
}

AudioStreamHandler::~AudioStreamHandler() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (IsInitialized()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&AudioStreamContainer::Stop,
                                          base::Unretained(stream_.get())));
    task_runner_->DeleteSoon(FROM_HERE, stream_.release());
  }
}

bool AudioStreamHandler::IsInitialized() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return !!stream_;
}

bool AudioStreamHandler::Play() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!IsInitialized())
    return false;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&AudioStreamContainer::Play),
                                base::Unretained(stream_.get())));
  return true;
}

void AudioStreamHandler::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!IsInitialized())
    return;

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AudioStreamContainer::Stop,
                                        base::Unretained(stream_.get())));
}

base::TimeDelta AudioStreamHandler::duration() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return duration_;
}

// static
void AudioStreamHandler::SetObserverForTesting(TestObserver* observer) {
  g_observer_for_testing = observer;
}

}  // namespace audio
