// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_RECURSIVE_CROSS_PROCESS_LOCK_POSIX_H_
#define RLZ_LIB_RECURSIVE_CROSS_PROCESS_LOCK_POSIX_H_

#include <pthread.h>

namespace base {
class FilePath;
}

namespace rlz_lib {

// Creating a recursive cross-process mutex on Windows is one line. On POSIX,
// there's no primitive for that, so this lock is emulated by an in-process
// mutex to get the recursive part, followed by a cross-process lock for the
// cross-process part.
// This is a struct so that it doesn't need a static initializer.
struct RecursiveCrossProcessLock {
  // Tries to acquire a recursive cross-process lock. Note that this _always_
  // acquires the in-process lock (if it wasn't already acquired). The parent
  // directory of |lock_file| must exist.
  bool TryGetCrossProcessLock(const base::FilePath& lock_filename);

  // Releases the lock. Should always be called, even if
  // TryGetCrossProcessLock() returned |false|.
  void ReleaseLock();

  pthread_mutex_t recursive_lock_;
  pthread_t locking_thread_;

  int file_lock_;
};

// On Mac, PTHREAD_RECURSIVE_MUTEX_INITIALIZER doesn't exist before 10.7 and
// is buggy on 10.7 (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=51906#c34),
// so emulate recursive locking with a normal non-recursive mutex.
#define RECURSIVE_CROSS_PROCESS_LOCK_INITIALIZER    \
  { PTHREAD_MUTEX_INITIALIZER, 0, -1 }

}  // namespace rlz_lib

#endif  // RLZ_LIB_RECURSIVE_CROSS_PROCESS_LOCK_POSIX_H_
