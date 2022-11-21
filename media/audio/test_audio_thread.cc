// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/test_audio_thread.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

namespace media {

TestAudioThread::TestAudioThread() : TestAudioThread(false) {}

TestAudioThread::TestAudioThread(bool use_real_thread) {
  if (use_real_thread) {
    thread_ = std::make_unique<base::Thread>("AudioThread");
#if BUILDFLAG(IS_WIN)
    thread_->init_com_with_mta(true);
#endif
    CHECK(thread_->Start());
    task_runner_ = thread_->task_runner();
  } else {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }
}

TestAudioThread::~TestAudioThread() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void TestAudioThread::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (thread_)
    thread_->Stop();
  else
    base::RunLoop().RunUntilIdle();
}

bool TestAudioThread::IsHung() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return false;
}

base::SingleThreadTaskRunner* TestAudioThread::GetTaskRunner() {
  return task_runner_.get();
}

base::SingleThreadTaskRunner* TestAudioThread::GetWorkerTaskRunner() {
  return task_runner_.get();
}

}  // namespace media
