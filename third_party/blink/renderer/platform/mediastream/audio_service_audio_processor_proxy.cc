// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/audio_service_audio_processor_proxy.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "media/base/audio_processor_controls.h"

namespace blink {

AudioServiceAudioProcessorProxy::AudioServiceAudioProcessorProxy()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

AudioServiceAudioProcessorProxy::~AudioServiceAudioProcessorProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  Stop();
}

void AudioServiceAudioProcessorProxy::SetControls(
    media::AudioProcessorControls* controls) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(!processor_controls_);
  DCHECK(controls);
  processor_controls_ = controls;

  stats_update_timer_.Start(
      FROM_HERE, kStatsUpdateInterval,
      base::BindRepeating(&AudioServiceAudioProcessorProxy::RequestStats,
                          weak_this_));
}

void AudioServiceAudioProcessorProxy::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  stats_update_timer_.Stop();
  if (processor_controls_) {
    processor_controls_ = nullptr;
  }
}

webrtc::AudioProcessorInterface::AudioProcessorStatistics
AudioServiceAudioProcessorProxy::GetStats(bool has_remote_tracks) {
  base::AutoLock lock(stats_lock_);
  // |has_remote_tracks| is ignored (not in use any more).
  return latest_stats_;
}

void AudioServiceAudioProcessorProxy::MaybeUpdateNumPreferredCaptureChannels(
    uint32_t num_channels) {
  if (num_preferred_capture_channels_ >= num_channels)
    return;

  num_preferred_capture_channels_ = num_channels;

  // Posting the task only when update is needed, to avoid spamming the main
  // thread.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioServiceAudioProcessorProxy::
                                    SetPreferredNumCaptureChannelsOnMainThread,
                                weak_this_, num_channels));
}

void AudioServiceAudioProcessorProxy::RequestStats() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (processor_controls_) {
    processor_controls_->GetStats(base::BindOnce(
        &AudioServiceAudioProcessorProxy::UpdateStats, weak_this_));
  }
}

void AudioServiceAudioProcessorProxy::UpdateStats(
    const media::AudioProcessingStats& new_stats) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  base::AutoLock lock(stats_lock_);
  latest_stats_.apm_statistics.echo_return_loss = new_stats.echo_return_loss;
  latest_stats_.apm_statistics.echo_return_loss_enhancement =
      new_stats.echo_return_loss_enhancement;
}

void AudioServiceAudioProcessorProxy::
    SetPreferredNumCaptureChannelsOnMainThread(uint32_t num_channels) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (processor_controls_) {
    processor_controls_->SetPreferredNumCaptureChannels(num_channels);
  }
}

}  // namespace blink
