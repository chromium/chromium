// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/owning_audio_manager_accessor.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_thread.h"
#include "media/audio/audio_thread_hang_monitor.h"
#include "services/audio/realtime_audio_thread.h"

using HangAction = media::AudioThreadHangMonitor::HangAction;

namespace audio {

namespace {

// Ideally, this would be based on the incoming audio's buffer durations.
// However, we might deal with multiple streams, with multiple buffer durations.
// Using a 10ms constant instead is acceptable (and better than the default)
// since there are no super-strict realtime requirements (no system audio calls
// waiting on these threads).
constexpr base::TimeDelta kReatimeThreadPeriod = base::Milliseconds(10);

HangAction GetAudioThreadHangAction() {
  return base::FeatureList::IsEnabled(features::kDumpOnAudioServiceHang)
             ? HangAction::kDumpAndTerminateCurrentProcess
             : HangAction::kTerminateCurrentProcess;
}

// Thread class for hosting owned AudioManager on the main thread of the
// service, with a separate worker thread (started on-demand) for running things
// that shouldn't be blocked by main-thread tasks.
class MainThread final : public media::AudioThread {
 public:
  MainThread();

  MainThread(const MainThread&) = delete;
  MainThread& operator=(const MainThread&) = delete;

  ~MainThread() final;

  // AudioThread implementation.
  void Stop() final;
  bool IsHung() const final;
  base::SingleThreadTaskRunner* GetTaskRunner() final;
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() final;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This is not started until the first time GetWorkerTaskRunner() is called.
  RealtimeAudioThread worker_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  media::AudioThreadHangMonitor::Ptr hang_monitor_;
};

MainThread::MainThread()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      worker_thread_("AudioWorkerThread", kReatimeThreadPeriod),
      hang_monitor_(media::AudioThreadHangMonitor::Create(
          GetAudioThreadHangAction(),
          /*use the default*/ std::nullopt,
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
    base::Thread::Options options;
    options.thread_type = base::ThreadType::kRealtimeAudio;
    CHECK(worker_thread_.StartWithOptions(std::move(options)));
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
    audio_manager_ =
        std::move(audio_manager_factory_cb_)
            .Run(std::make_unique<MainThread>(), log_factory_.get());
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
