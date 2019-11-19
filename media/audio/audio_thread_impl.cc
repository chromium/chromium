// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_thread_impl.h"

#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "media/audio/audio_thread_hang_monitor.h"

namespace media {

AudioThreadImpl::AudioThreadImpl()
    : thread_("AudioThread"),
      hang_monitor_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  base::Thread::Options thread_options;
#if defined(OS_WIN)
  thread_.init_com_with_mta(true);
#elif defined(OS_FUCHSIA)
  // FIDL-based APIs require async_t, which is initialized on IO thread.
  thread_options.message_pump_type = base::MessagePumpType::IO;
#endif
  CHECK(thread_.StartWithOptions(thread_options));

#if defined(OS_MACOSX)
  // On Mac, the audio task runner must belong to the main thread.
  // See http://crbug.com/158170.
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
#else
  task_runner_ = thread_.task_runner();
#endif
  worker_task_runner_ = thread_.task_runner();

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Since we run on the main thread on Mac, we don't need a hang monitor.
  // https://crbug.com/946968: The hang monitor possibly causes crashes on
  // Android
  hang_monitor_ = AudioThreadHangMonitor::Create(
      AudioThreadHangMonitor::HangAction::kDoNothing, base::nullopt,
      base::DefaultTickClock::GetInstance(), task_runner_);
#endif
}

AudioThreadImpl::~AudioThreadImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void AudioThreadImpl::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  hang_monitor_.reset();

  // Note that on MACOSX, we can still have tasks posted on the |task_runner_|,
  // since it is the main thread task runner and we do not stop the main thread.
  // But this is fine becuase none of those tasks will actually run.
  thread_.Stop();
}

bool AudioThreadImpl::IsHung() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return hang_monitor_ ? hang_monitor_->IsAudioThreadHung() : false;
}

base::SingleThreadTaskRunner* AudioThreadImpl::GetTaskRunner() {
  return task_runner_.get();
}

base::SingleThreadTaskRunner* AudioThreadImpl::GetWorkerTaskRunner() {
  return worker_task_runner_.get();
}

}  // namespace media
