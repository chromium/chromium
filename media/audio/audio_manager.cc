// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media_switches.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace media {
namespace {

// The singleton instance of AudioManager. This is set when Create() is called.
AudioManager* g_last_created = nullptr;

// Maximum number of failed pings to the audio thread allowed. A UMA will be
// recorded once this count is reached; if enabled, a non-crash dump will be
// captured as well. We require at least three failed pings before recording to
// ensure unobservable power events aren't mistakenly caught (e.g., the system
// suspends before a OnSuspend() event can be fired).
const int kMaxFailedPingsCount = 3;

// Helper class for managing global AudioManager data and hang monitor. If the
// audio thread is hung for > |kMaxFailedPingsCount| * |max_hung_task_time_|, we
// want to record a UMA and optionally a non-crash dump to find offenders in the
// field.
class AudioManagerHelper : public base::PowerObserver {
 public:
  // These values are histogrammed over time; do not change their ordinal
  // values.
  enum ThreadStatus {
    THREAD_NONE = 0,
    THREAD_STARTED,
    THREAD_HUNG,
    THREAD_RECOVERED,
    THREAD_MAX = THREAD_RECOVERED
  };

  AudioManagerHelper() = default;
  ~AudioManagerHelper() override = default;

  void StartHangTimer(
      scoped_refptr<base::SingleThreadTaskRunner> monitor_task_runner) {
    CHECK(!monitor_task_runner_);
    CHECK(!audio_task_runner_);
    monitor_task_runner_ = std::move(monitor_task_runner);
    audio_task_runner_ = AudioManager::Get()->GetTaskRunner();
    base::PowerMonitor::Get()->AddObserver(this);

    io_task_running_ = audio_task_running_ = true;
    audio_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                   base::Unretained(this)));
    monitor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AudioManagerHelper::RecordAudioThreadStatus,
                                  base::Unretained(this)));
  }

  bool IsAudioThreadHung() {
    base::AutoLock lock(hang_lock_);
    return audio_thread_status_ == THREAD_HUNG;
  }

  base::SingleThreadTaskRunner* monitor_task_runner() const {
    return monitor_task_runner_.get();
  }

  AudioLogFactory* fake_log_factory() { return &fake_log_factory_; }

#if defined(OS_WIN)
  // This should be called before creating an AudioManager in tests to ensure
  // that the creating thread is COM initialized.
  void InitializeCOMForTesting() {
    com_initializer_for_testing_.reset(new base::win::ScopedCOMInitializer());
  }
#endif

#if defined(OS_LINUX)
  void set_app_name(const std::string& app_name) { app_name_ = app_name; }
  const std::string& app_name() const { return app_name_; }
#endif

 private:
  // base::PowerObserver overrides.
  // Disable hang detection when the system goes into the suspend state.
  void OnSuspend() override {
    base::AutoLock lock(hang_lock_);
    hang_detection_enabled_ = false;
    failed_pings_ = successful_pings_ = 0;
  }
  // Reenable hang detection once the system comes out of the suspend state.
  void OnResume() override {
    base::AutoLock lock(hang_lock_);
    hang_detection_enabled_ = true;
    last_audio_thread_timer_tick_ = base::TimeTicks::Now();
    failed_pings_ = successful_pings_ = 0;

    // If either of the tasks were stopped during suspend, start them now.
    if (!audio_task_running_) {
      audio_task_running_ = true;

      base::AutoUnlock unlock(hang_lock_);
      audio_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                     base::Unretained(this)));
    }

    if (!io_task_running_) {
      io_task_running_ = true;

      base::AutoUnlock unlock(hang_lock_);
      monitor_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AudioManagerHelper::RecordAudioThreadStatus,
                         base::Unretained(this)));
    }
  }

  // Runs on |monitor_task_runner|.
  void RecordAudioThreadStatus() {
    DCHECK(monitor_task_runner_->BelongsToCurrentThread());
    {
      base::AutoLock lock(hang_lock_);

      // Don't attempt to verify the tick time or post our task if the system is
      // in the process of suspending or resuming.
      if (!hang_detection_enabled_) {
        io_task_running_ = false;
        return;
      }

      DCHECK(io_task_running_);
      const base::TimeTicks now = base::TimeTicks::Now();
      const base::TimeDelta tick_delta = now - last_audio_thread_timer_tick_;
      if (tick_delta > max_hung_task_time_) {
        successful_pings_ = 0;
        if (++failed_pings_ >= kMaxFailedPingsCount &&
            audio_thread_status_ < THREAD_HUNG) {
          HistogramThreadStatus(THREAD_HUNG);
        }
      } else {
        failed_pings_ = 0;
        ++successful_pings_;
        if (audio_thread_status_ == THREAD_NONE) {
          HistogramThreadStatus(THREAD_STARTED);
        } else if (audio_thread_status_ == THREAD_HUNG &&
                   successful_pings_ >= kMaxFailedPingsCount) {
          // Require just as many successful pings to recover from failure.
          HistogramThreadStatus(THREAD_RECOVERED);
        }
      }
    }

    // Don't hold the lock while posting the next task.
    monitor_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerHelper::RecordAudioThreadStatus,
                       base::Unretained(this)),
        max_hung_task_time_);
  }

  // Runs on the audio thread.
  void UpdateLastAudioThreadTimeTick() {
    DCHECK(audio_task_runner_->BelongsToCurrentThread());
    {
      base::AutoLock lock(hang_lock_);
      last_audio_thread_timer_tick_ = base::TimeTicks::Now();
      failed_pings_ = 0;

      // Don't post our task if the system is or will be suspended.
      if (!hang_detection_enabled_) {
        audio_task_running_ = false;
        return;
      }

      DCHECK(audio_task_running_);
    }

    // Don't hold the lock while posting the next task.
    audio_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AudioManagerHelper::UpdateLastAudioThreadTimeTick,
                   base::Unretained(this)),
        max_hung_task_time_ / 5);
  }

  void HistogramThreadStatus(ThreadStatus status) {
    DCHECK(monitor_task_runner_->BelongsToCurrentThread());
    hang_lock_.AssertAcquired();
    audio_thread_status_ = status;
    UMA_HISTOGRAM_ENUMERATION("Media.AudioThreadStatus", audio_thread_status_,
                              THREAD_MAX + 1);
  }

  FakeAudioLogFactory fake_log_factory_;

  const base::TimeDelta max_hung_task_time_ = base::TimeDelta::FromMinutes(1);
  scoped_refptr<base::SingleThreadTaskRunner> monitor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  base::Lock hang_lock_;
  bool hang_detection_enabled_ GUARDED_BY(hang_lock_) = true;
  base::TimeTicks last_audio_thread_timer_tick_ GUARDED_BY(hang_lock_);
  uint32_t failed_pings_ = 0;
  bool io_task_running_ = false;
  bool audio_task_running_ = false;
  ThreadStatus audio_thread_status_ = THREAD_NONE;
  uint32_t successful_pings_ = 0;

#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_for_testing_;
#endif

#if defined(OS_LINUX)
  std::string app_name_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AudioManagerHelper);
};

AudioManagerHelper* GetHelper() {
  static AudioManagerHelper* helper = new AudioManagerHelper();
  return helper;
}

}  // namespace

// Forward declaration of the platform specific AudioManager factory function.
std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory);

void AudioManager::SetMaxStreamCountForTesting(int max_input, int max_output) {
  NOTREACHED();
}

AudioManager::AudioManager(std::unique_ptr<AudioThread> audio_thread)
    : audio_thread_(std::move(audio_thread)) {
  DCHECK(audio_thread_);

  if (g_last_created) {
    // We create multiple instances of AudioManager only when testing.
    // We should not encounter this case in production.
    LOG(WARNING) << "Multiple instances of AudioManager detected";
  }
  // We always override |g_last_created| irrespective of whether it is already
  // set or not becuase it represents the last created instance.
  g_last_created = this;
}

AudioManager::~AudioManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(shutdown_);

  if (g_last_created == this) {
    g_last_created = nullptr;
  } else {
    // We create multiple instances of AudioManager only when testing.
    // We should not encounter this case in production.
    LOG(WARNING) << "Multiple instances of AudioManager detected";
  }
}

// static
std::unique_ptr<AudioManager> AudioManager::Create(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  std::unique_ptr<AudioManager> manager =
      CreateAudioManager(std::move(audio_thread), audio_log_factory);
  manager->InitializeDebugRecording();
  return manager;
}

// static
std::unique_ptr<AudioManager> AudioManager::CreateForTesting(
    std::unique_ptr<AudioThread> audio_thread) {
#if defined(OS_WIN)
  GetHelper()->InitializeCOMForTesting();
#endif
  return Create(std::move(audio_thread), GetHelper()->fake_log_factory());
}

// static
void AudioManager::StartHangMonitorIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (GetHelper()->monitor_task_runner())
    return;

  DCHECK(AudioManager::Get());
  DCHECK(task_runner);
  DCHECK_NE(task_runner, AudioManager::Get()->GetTaskRunner());

  GetHelper()->StartHangTimer(std::move(task_runner));
}

#if defined(OS_LINUX)
// static
void AudioManager::SetGlobalAppName(const std::string& app_name) {
  GetHelper()->set_app_name(app_name);
}

// static
const std::string& AudioManager::GetGlobalAppName() {
  return GetHelper()->app_name();
}
#endif

// static
AudioManager* AudioManager::Get() {
  return g_last_created;
}

bool AudioManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Do not attempt to stop the audio thread if it is hung.
  // Otherwise the current thread will hang too: crbug.com/729494
  // TODO(olka, grunell): Will be fixed when audio is its own process.
  if (GetHelper()->IsAudioThreadHung())
    return false;

  // TODO(alokp): Suspend hang monitor.
  if (audio_thread_->GetTaskRunner()->BelongsToCurrentThread()) {
    ShutdownOnAudioThread();
  } else {
    audio_thread_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioManager::ShutdownOnAudioThread,
                                  base::Unretained(this)));
  }
  audio_thread_->Stop();
  shutdown_ = true;
  return true;
}

void AudioManager::SetDiverterCallbacks(
    AddDiverterCallback add_callback,
    RemoveDiverterCallback remove_callback) {
  add_diverter_callback_ = std::move(add_callback);
  remove_diverter_callback_ = std::move(remove_callback);
}

void AudioManager::AddDiverter(const base::UnguessableToken& group_id,
                               media::AudioSourceDiverter* diverter) {
  if (!add_diverter_callback_.is_null())
    add_diverter_callback_.Run(group_id, diverter);
}

void AudioManager::RemoveDiverter(media::AudioSourceDiverter* diverter) {
  if (!remove_diverter_callback_.is_null())
    remove_diverter_callback_.Run(diverter);
}

}  // namespace media
