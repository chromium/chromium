// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/audio_service_audio_processor_proxy.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
constexpr base::TimeDelta kMaxStatsInterval = base::TimeDelta::FromSeconds(5);
constexpr base::TimeDelta kMinStatsInterval =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

AudioServiceAudioProcessorProxy::AudioServiceAudioProcessorProxy(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_runner_(std::move(main_thread_task_runner)),
      target_stats_interval_(kMaxStatsInterval) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
}

AudioServiceAudioProcessorProxy::~AudioServiceAudioProcessorProxy() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  Stop();
}

void AudioServiceAudioProcessorProxy::Stop() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  aec_dump_agent_impl_.reset();

  if (processor_controls_) {
    processor_controls_->StopEchoCancellationDump();
    processor_controls_ = nullptr;
  }

  stats_update_timer_.Stop();
}

void AudioServiceAudioProcessorProxy::OnStartDump(base::File dump_file) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(dump_file.IsValid());
  if (processor_controls_) {
    processor_controls_->StartEchoCancellationDump(std::move(dump_file));
  } else {
    // Post the file close to avoid blocking the main thread.
    worker_pool::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::LOWEST, base::MayBlock()},
        CrossThreadBindOnce([](base::File) {}, std::move(dump_file)));
  }
}

void AudioServiceAudioProcessorProxy::OnStopDump() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (processor_controls_)
    processor_controls_->StopEchoCancellationDump();
}

void AudioServiceAudioProcessorProxy::SetControls(
    media::AudioProcessorControls* controls) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(!processor_controls_);
  DCHECK(controls);
  processor_controls_ = controls;

  // Initialize the stats interval request timer with the current time ticks,
  // so it makes any sort of sense.
  last_stats_request_time_ = base::TimeTicks::Now();
  stats_update_timer_.SetTaskRunner(main_thread_runner_);
  RescheduleStatsUpdateTimer(target_stats_interval_);

  // Can be null in unit tests. That's okay.
  aec_dump_agent_impl_ = AecDumpAgentImpl::Create(this);
}

webrtc::AudioProcessorInterface::AudioProcessorStatistics
AudioServiceAudioProcessorProxy::GetStats(bool has_remote_tracks) {
  base::AutoLock lock(stats_lock_);
  // Find some reasonable update interval, rounding down to the nearest one
  // tenth of a second. The update interval is chosen so that the rate of
  // updates we get from the audio service is near the interval at which the
  // client calls GetStats.
  const auto rounded = [](base::TimeDelta d) {
    return d - (d % base::TimeDelta::FromMilliseconds(100));
  };
  const auto now = base::TimeTicks::Now();
  const auto request_interval = rounded(now - last_stats_request_time_);
  target_stats_interval_ = std::max(
      kMinStatsInterval, std::min(request_interval, kMaxStatsInterval));

  last_stats_request_time_ = now;

  // |has_remote_tracks| is ignored, since the remote AudioProcessingModule gets
  // this information more directly.
  return latest_stats_;
}

void AudioServiceAudioProcessorProxy::RescheduleStatsUpdateTimer(
    base::TimeDelta new_interval) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  // Unretained is safe since |this| owns |stats_update_timer_|.
  stats_update_timer_.Start(
      FROM_HERE, new_interval,
      base::BindRepeating(&AudioServiceAudioProcessorProxy::RequestStats,
                          base::Unretained(this)));
}

void AudioServiceAudioProcessorProxy::RequestStats() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (processor_controls_) {
    processor_controls_->GetStats(
        base::BindOnce(&AudioServiceAudioProcessorProxy::UpdateStats,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void AudioServiceAudioProcessorProxy::UpdateStats(
    const AudioProcessorStatistics& new_stats) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  base::TimeDelta target_interval;
  {
    base::AutoLock lock(stats_lock_);
    latest_stats_ = new_stats;
    target_interval = target_stats_interval_;
  }

  if (target_interval != stats_update_timer_.GetCurrentDelay()) {
    RescheduleStatsUpdateTimer(target_interval);
  }
}

}  // namespace blink
