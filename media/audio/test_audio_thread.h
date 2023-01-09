// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_TEST_AUDIO_THREAD_H_
#define MEDIA_AUDIO_TEST_AUDIO_THREAD_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_thread.h"

namespace media {

class TestAudioThread final : public AudioThread {
 public:
  TestAudioThread();
  explicit TestAudioThread(bool use_real_thread);

  TestAudioThread(const TestAudioThread&) = delete;
  TestAudioThread& operator=(const TestAudioThread&) = delete;

  ~TestAudioThread() final;

  // AudioThread implementation.
  void Stop() final;
  bool IsHung() const final;
  base::SingleThreadTaskRunner* GetTaskRunner() final;
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() final;

 private:
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_TEST_AUDIO_THREAD_H_
