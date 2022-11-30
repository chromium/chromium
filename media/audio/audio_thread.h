// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_THREAD_H_
#define MEDIA_AUDIO_AUDIO_THREAD_H_

#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// This class encapulates the logic for the thread and task runners that the
// AudioManager and related classes run on.
class MEDIA_EXPORT AudioThread {
 public:
  virtual ~AudioThread() {}

  // Synchronously stops all underlying threads.
  virtual void Stop() = 0;

  // Indicates whether the audio thread is responsive. If false, calling Stop()
  // will likely block forever.
  virtual bool IsHung() const = 0;

  // Returns the task runner used for audio IO.
  // It always returns a non-null task runner (even after Stop has been called).
  virtual base::SingleThreadTaskRunner* GetTaskRunner() = 0;

  // Heavyweight tasks should use GetWorkerTaskRunner() instead of
  // GetTaskRunner(). On most platforms they are the same, but some share the
  // UI loop with the audio IO loop.
  // It always returns a non-null task runner (even after Stop has been called).
  virtual base::SingleThreadTaskRunner* GetWorkerTaskRunner() = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_THREAD_H_
