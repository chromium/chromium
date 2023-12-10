// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/recursive_cross_process_lock_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <tuple>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace rlz_lib {

bool RecursiveCrossProcessLock::TryGetCrossProcessLock(
    const base::FilePath& lock_filename) {
  bool just_got_lock = false;

  // Emulate a recursive mutex with a non-recursive one.
  if (pthread_mutex_trylock(&recursive_lock_) == EBUSY) {
    if (pthread_equal(pthread_self(), locking_thread_) == 0) {
      // Some other thread has the lock, wait for it.
      pthread_mutex_lock(&recursive_lock_);
      CHECK(locking_thread_ == 0);
      just_got_lock = true;
    }
  } else {
    just_got_lock = true;
  }

  locking_thread_ = pthread_self();

  // Try to acquire file lock.
  if (just_got_lock) {
    const int kMaxTimeoutMS = 5000;  // Matches Windows.
    const int kSleepPerTryMS = 200;

    CHECK(file_lock_ == -1);
    file_lock_ = open(lock_filename.value().c_str(), O_RDWR | O_CREAT, 0666);
    if (file_lock_ == -1) {
      VPLOG(1) << "Failed to open: " << lock_filename.value();
      return false;
    }

    int flock_result = -1;
    int elapsed_ms = 0;
    while ((flock_result =
               HANDLE_EINTR(flock(file_lock_, LOCK_EX | LOCK_NB))) == -1 &&
           errno == EWOULDBLOCK &&
           elapsed_ms < kMaxTimeoutMS) {
      usleep(kSleepPerTryMS * 1000);
      elapsed_ms += kSleepPerTryMS;
    }

    if (flock_result == -1) {
      VPLOG(1) << "Failed flock: " << lock_filename.value();
      close(file_lock_);
      file_lock_ = -1;
      return false;
    }
    return true;
  } else {
    return file_lock_ != -1;
  }
}

void RecursiveCrossProcessLock::ReleaseLock() {
  if (file_lock_ != -1) {
    std::ignore = HANDLE_EINTR(flock(file_lock_, LOCK_UN));
    close(file_lock_);
    file_lock_ = -1;
  }

  locking_thread_ = 0;
  pthread_mutex_unlock(&recursive_lock_);
}

}  // namespace rlz_lib
