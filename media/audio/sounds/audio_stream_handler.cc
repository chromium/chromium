// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sounds/audio_stream_handler.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/sounds/wav_audio_handler.h"
#include "media/base/channel_layout.h"

namespace media {

namespace {

// Volume percent.
const double kOutputVolumePercent = 0.8;

// The number of frames each OnMoreData() call will request.
const int kDefaultFrameCount = 1024;

// Keep alive timeout for audio stream.
const int kKeepAliveMs = 1500;

AudioStreamHandler::TestObserver* g_observer_for_testing = NULL;
AudioOutputStream::AudioSourceCallback* g_audio_source_for_testing = NULL;

}  // namespace

class AudioStreamHandler::AudioStreamContainer
    : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit AudioStreamContainer(std::unique_ptr<WavAudioHandler> wav_audio)
      : audio_manager_(AudioManager::Get()),
        started_(false),
        stream_(NULL),
        cursor_(0),
        delayed_stop_posted_(false),
        wav_audio_(std::move(wav_audio)) {
    DCHECK(audio_manager_);
    DCHECK(wav_audio_);
  }

  ~AudioStreamContainer() override {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  }

  void Play() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    if (!stream_) {
      const AudioParameters params(
          AudioParameters::AUDIO_PCM_LOW_LATENCY,
          GuessChannelLayout(wav_audio_->num_channels()),
          wav_audio_->sample_rate(), kDefaultFrameCount);
      stream_ =
          audio_manager_->MakeAudioOutputStreamProxy(params, std::string());
      if (!stream_ || !stream_->Open()) {
        LOG(ERROR) << "Failed to open an output stream.";
        return;
      }
      stream_->SetVolume(kOutputVolumePercent);
    }

    {
      base::AutoLock al(state_lock_);

      delayed_stop_posted_ = false;
      stop_closure_.Reset(base::Bind(&AudioStreamContainer::StopStream,
                                     base::Unretained(this)));

      if (started_) {
        if (wav_audio_->AtEnd(cursor_))
          cursor_ = 0;
        return;
      }

      cursor_ = 0;
    }

    started_ = true;
    if (g_audio_source_for_testing)
      stream_->Start(g_audio_source_for_testing);
    else
      stream_->Start(this);

    if (g_observer_for_testing)
      g_observer_for_testing->OnPlay();
  }

  void Stop() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    StopStream();
    if (stream_)
      stream_->Close();
    stream_ = NULL;
    stop_closure_.Cancel();
  }

 private:
  // AudioOutputStream::AudioSourceCallback overrides:
  // Following methods could be called from *ANY* thread.
  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 int /* prior_frames_skipped */,
                 AudioBus* dest) override {
    base::AutoLock al(state_lock_);
    size_t bytes_written = 0;

    if (wav_audio_->AtEnd(cursor_) ||
        !wav_audio_->CopyTo(dest, cursor_, &bytes_written)) {
      if (delayed_stop_posted_)
        return 0;
      delayed_stop_posted_ = true;
      audio_manager_->GetTaskRunner()->PostDelayedTask(
          FROM_HERE, stop_closure_.callback(),
          base::TimeDelta::FromMilliseconds(kKeepAliveMs));
      return 0;
    }
    cursor_ += bytes_written;
    return dest->frames();
  }

  void OnError() override {
    LOG(ERROR) << "Error during system sound reproduction.";
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&AudioStreamContainer::Stop, base::Unretained(this)));
  }

  void StopStream() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    if (stream_ && started_) {
      // Do not hold the |state_lock_| while stopping the output stream.
      stream_->Stop();
      if (g_observer_for_testing)
        g_observer_for_testing->OnStop(cursor_);
    }

    started_ = false;
  }

  // Must only be accessed on the AudioManager::GetTaskRunner() thread.
  AudioManager* const audio_manager_;
  bool started_;
  AudioOutputStream* stream_;

  // All variables below must be accessed under |state_lock_| when |started_|.
  base::Lock state_lock_;
  size_t cursor_;
  bool delayed_stop_posted_;
  std::unique_ptr<WavAudioHandler> wav_audio_;
  base::CancelableClosure stop_closure_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamContainer);
};

AudioStreamHandler::AudioStreamHandler(const base::StringPiece& wav_data) {
  AudioManager* manager = AudioManager::Get();
  if (!manager) {
    LOG(ERROR) << "Can't get access to audio manager.";
    return;
  }

  std::unique_ptr<WavAudioHandler> wav_audio =
      WavAudioHandler::Create(wav_data);
  if (!wav_audio) {
    LOG(ERROR) << "wav_data is not valid";
    return;
  }

  const AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               GuessChannelLayout(wav_audio->num_channels()),
                               wav_audio->sample_rate(), kDefaultFrameCount);
  if (!params.IsValid()) {
    LOG(ERROR) << "Audio params are invalid.";
    return;
  }

  // Store the duration of the WAV data then pass the handler to |stream_|.
  duration_ = wav_audio->GetDuration();
  stream_.reset(new AudioStreamContainer(std::move(wav_audio)));
}

AudioStreamHandler::~AudioStreamHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsInitialized()) {
    AudioManager::Get()->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioStreamContainer::Stop,
                                  base::Unretained(stream_.get())));
    AudioManager::Get()->GetTaskRunner()->DeleteSoon(FROM_HERE,
                                                     stream_.release());
  }
}

bool AudioStreamHandler::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!stream_;
}

bool AudioStreamHandler::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsInitialized())
    return false;

  AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioStreamContainer::Play),
                 base::Unretained(stream_.get())));
  return true;
}

void AudioStreamHandler::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsInitialized())
    return;

  AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioStreamContainer::Stop, base::Unretained(stream_.get())));
}

base::TimeDelta AudioStreamHandler::duration() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return duration_;
}

// static
void AudioStreamHandler::SetObserverForTesting(TestObserver* observer) {
  g_observer_for_testing = observer;
}

// static
void AudioStreamHandler::SetAudioSourceForTesting(
    AudioOutputStream::AudioSourceCallback* source) {
  g_audio_source_for_testing = source;
}

}  // namespace media
