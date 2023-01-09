// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_THREAD_IMPL_H_
#define MEDIA_AUDIO_AUDIO_THREAD_IMPL_H_

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_thread.h"
#include "media/audio/audio_thread_hang_monitor.h"

namespace media {

class MEDIA_EXPORT AudioThreadImpl final : public AudioThread {
 public:
  AudioThreadImpl();

  AudioThreadImpl(const AudioThreadImpl&) = delete;
  AudioThreadImpl& operator=(const AudioThreadImpl&) = delete;

  ~AudioThreadImpl() final;

  // AudioThread implementation.
  void Stop() final;
  bool IsHung() const final;
  base::SingleThreadTaskRunner* GetTaskRunner() final;
  base::SingleThreadTaskRunner* GetWorkerTaskRunner() final;

 private:
  base::Thread thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  // Null on Mac OS, initialized in the constructor on other platforms.
  AudioThreadHangMonitor::Ptr hang_monitor_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_THREAD_IMPL_H_
