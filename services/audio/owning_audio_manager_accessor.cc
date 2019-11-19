// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/owning_audio_manager_accessor.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_thread.h"
#include "media/audio/audio_thread_hang_monitor.h"

using HangAction = media::AudioThreadHangMonitor::HangAction;

namespace audio {

namespace {

base::Optional<base::TimeDelta> GetAudioThreadHangDeadline() {
  if (!base::FeatureList::IsEnabled(
          features::kAudioServiceOutOfProcessKillAtHang)) {
    return base::nullopt;
  }
  const std::string timeout_string = base::GetFieldTrialParamValueByFeature(
      features::kAudioServiceOutOfProcessKillAtHang, "timeout_seconds");
  int timeout_int = 0;
  if (!base::StringToInt(timeout_string, &timeout_int) || timeout_int == 0)
    return base::nullopt;
  return base::TimeDelta::FromSeconds(timeout_int);
}

HangAction GetAudioThreadHangAction() {
  const bool dump =
      base::FeatureList::IsEnabled(features::kDumpOnAudioServiceHang);
  const bool kill = base::FeatureList::IsEnabled(
      features::kAudioServiceOutOfProcessKillAtHang);
  if (dump) {
    return kill ? HangAction::kDumpAndTerminateCurrentProcess
                : HangAction::kDump;
  }
  return kill ? HangAction::kTerminateCurrentProcess : HangAction::kDoNothing;
}

// Thread class for hosting owned AudioManager on the main thread of the
// service, with a separate worker thread (started on-demand) for running things
// that shouldn't be blocked by main-thread tasks.
class MainThread final : public media::AudioThread {
 public:
  MainThread();
  ~MainThread() final;

  // AudioThread implementation.
  void Stop() final;
  bool IsHung() const final;
  base::SingleThreadTaskRunner* GetTaskRunner() final;
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() final;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This is not started until the first time GetWorkerTaskRunner() is called.
  base::Thread worker_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  media::AudioThreadHangMonitor::Ptr hang_monitor_;

  DISALLOW_COPY_AND_ASSIGN(MainThread);
};

MainThread::MainThread()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      worker_thread_("AudioWorkerThread"),
      hang_monitor_(media::AudioThreadHangMonitor::Create(
          GetAudioThreadHangAction(),
          GetAudioThreadHangDeadline(),
          base::DefaultTickClock::GetInstance(),
          task_runner_)) {}

MainThread::~MainThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void MainThread::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  hang_monitor_.reset();

  if (worker_task_runner_) {
    worker_task_runner_ = nullptr;
    worker_thread_.Stop();
  }
}

bool MainThread::IsHung() const {
  return hang_monitor_->IsAudioThreadHung();
}

base::SingleThreadTaskRunner* MainThread::GetTaskRunner() {
  return task_runner_.get();
}

base::SingleThreadTaskRunner* MainThread::GetWorkerTaskRunner() {
  DCHECK(
      task_runner_->BelongsToCurrentThread() ||
      (worker_task_runner_ && worker_task_runner_->BelongsToCurrentThread()));
  if (!worker_task_runner_) {
    CHECK(worker_thread_.Start());
    worker_task_runner_ = worker_thread_.task_runner();
  }
  return worker_task_runner_.get();
}

}  // namespace

OwningAudioManagerAccessor::OwningAudioManagerAccessor(
    AudioManagerFactoryCallback audio_manager_factory_cb)
    : audio_manager_factory_cb_(std::move(audio_manager_factory_cb)) {
  DCHECK(audio_manager_factory_cb_);
}

OwningAudioManagerAccessor::~OwningAudioManagerAccessor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

media::AudioManager* OwningAudioManagerAccessor::GetAudioManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!audio_manager_) {
    TRACE_EVENT0("audio", "AudioManager creation");
    DCHECK(audio_manager_factory_cb_);
    DCHECK(log_factory_);
    base::TimeTicks creation_start_time = base::TimeTicks::Now();
    audio_manager_ = std::move(audio_manager_factory_cb_)
                         .Run(std::make_unique<MainThread>(), log_factory_);
    DCHECK(audio_manager_);
    UMA_HISTOGRAM_TIMES("Media.AudioService.AudioManagerStartupTime",
                        base::TimeTicks::Now() - creation_start_time);
  }
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_.get();
}

void OwningAudioManagerAccessor::SetAudioLogFactory(
    media::AudioLogFactory* log_factory) {
  log_factory_ = log_factory;
}

void OwningAudioManagerAccessor::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (audio_manager_)
    audio_manager_->Shutdown();
  audio_manager_factory_cb_ = AudioManagerFactoryCallback();
}

}  // namespace audio
