// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace media {
namespace {

// The singleton instance of AudioManager. This is set when Create() is called.
AudioManager* g_last_created = nullptr;

// Helper class for managing global AudioManager data.
class AudioManagerHelper {
 public:
  AudioManagerHelper() = default;

  AudioManagerHelper(const AudioManagerHelper&) = delete;
  AudioManagerHelper& operator=(const AudioManagerHelper&) = delete;

  ~AudioManagerHelper() = default;

  AudioLogFactory* fake_log_factory() { return &fake_log_factory_; }

#if BUILDFLAG(IS_WIN)
  // This should be called before creating an AudioManager in tests to ensure
  // that the creating thread is COM initialized.
  void InitializeCOMForTesting() {
    com_initializer_for_testing_ =
        std::make_unique<base::win::ScopedCOMInitializer>();
  }
#endif

  void set_app_name(const std::string& app_name) { app_name_ = app_name; }
  const std::string& app_name() const { return app_name_; }

  FakeAudioLogFactory fake_log_factory_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_for_testing_;
#endif

  std::string app_name_;
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
  NOTREACHED_IN_MIGRATION();
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
  // set or not because it represents the last created instance.
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
#if BUILDFLAG(IS_WIN)
  GetHelper()->InitializeCOMForTesting();
#endif
  return Create(std::move(audio_thread), GetHelper()->fake_log_factory());
}

// static
void AudioManager::SetGlobalAppName(const std::string& app_name) {
  GetHelper()->set_app_name(app_name);
}

// static
const std::string& AudioManager::GetGlobalAppName() {
  return GetHelper()->app_name();
}

// static
AudioManager* AudioManager::Get() {
  return g_last_created;
}

bool AudioManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (audio_thread_->GetTaskRunner()->BelongsToCurrentThread()) {
    // If this is the audio thread, there is no need to check if it's hung
    // (since it's clearly not). https://crbug.com/919854.
    ShutdownOnAudioThread();
  } else {
    // Do not attempt to stop the audio thread if it is hung.
    // Otherwise the current thread will hang too: https://crbug.com/729494
    // TODO(olka, grunell): Will be fixed when audio is its own process.
    if (audio_thread_->IsHung())
      return false;

    audio_thread_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AudioManager::ShutdownOnAudioThread,
                                  base::Unretained(this)));
  }
  audio_thread_->Stop();
  shutdown_ = true;
  return true;
}

void AudioManager::TraceAmplitudePeak(bool trace_start) {
  base::AutoLock scoped_lock(tracing_lock_);

  constexpr char kTraceName[] = "AmplitudePeak";

  if (trace_start) {
    // We might have never closed the previous trace. Abort it now.
    if (is_trace_started_) {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          TRACE_DISABLED_BY_DEFAULT("audio.latency"), kTraceName,
          current_trace_id_, "aborted", true);
    }

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("audio.latency"), kTraceName,
        ++current_trace_id_);

    is_trace_started_ = true;
    return;
  }

  if (!is_trace_started_) {
    // Avoid ending traces that were never started.
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1(TRACE_DISABLED_BY_DEFAULT("audio.latency"),
                                  kTraceName, current_trace_id_, "aborted",
                                  false);

  is_trace_started_ = false;
}

}  // namespace media
