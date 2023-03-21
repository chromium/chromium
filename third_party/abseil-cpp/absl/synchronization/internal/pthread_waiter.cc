// Copyright 2023 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/synchronization/internal/pthread_waiter.h"

#ifdef ABSL_INTERNAL_HAVE_PTHREAD_WAITER

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>

#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/thread_identity.h"
#include "absl/base/optimization.h"
#include "absl/synchronization/internal/kernel_timeout.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace synchronization_internal {

namespace {
class PthreadMutexHolder {
 public:
  explicit PthreadMutexHolder(pthread_mutex_t *mu) : mu_(mu) {
    const int err = pthread_mutex_lock(mu_);
    if (err != 0) {
      ABSL_RAW_LOG(FATAL, "pthread_mutex_lock failed: %d", err);
    }
  }

  PthreadMutexHolder(const PthreadMutexHolder &rhs) = delete;
  PthreadMutexHolder &operator=(const PthreadMutexHolder &rhs) = delete;

  ~PthreadMutexHolder() {
    const int err = pthread_mutex_unlock(mu_);
    if (err != 0) {
      ABSL_RAW_LOG(FATAL, "pthread_mutex_unlock failed: %d", err);
    }
  }

 private:
  pthread_mutex_t *mu_;
};
}  // namespace

#ifdef ABSL_INTERNAL_NEED_REDUNDANT_CONSTEXPR_DECL
constexpr char PthreadWaiter::kName[];
#endif

PthreadWaiter::PthreadWaiter() : waiter_count_(0), wakeup_count_(0) {
  const int err = pthread_mutex_init(&mu_, 0);
  if (err != 0) {
    ABSL_RAW_LOG(FATAL, "pthread_mutex_init failed: %d", err);
  }

  const int err2 = pthread_cond_init(&cv_, 0);
  if (err2 != 0) {
    ABSL_RAW_LOG(FATAL, "pthread_cond_init failed: %d", err2);
  }
}

bool PthreadWaiter::Wait(KernelTimeout t) {
  struct timespec abs_timeout;
  if (t.has_timeout()) {
    abs_timeout = t.MakeAbsTimespec();
  }

  PthreadMutexHolder h(&mu_);
  ++waiter_count_;
  // Loop until we find a wakeup to consume or timeout.
  // Note that, since the thread ticker is just reset, we don't need to check
  // whether the thread is idle on the very first pass of the loop.
  bool first_pass = true;
  while (wakeup_count_ == 0) {
    if (!first_pass) MaybeBecomeIdle();
    // No wakeups available, time to wait.
    if (!t.has_timeout()) {
      const int err = pthread_cond_wait(&cv_, &mu_);
      if (err != 0) {
        ABSL_RAW_LOG(FATAL, "pthread_cond_wait failed: %d", err);
      }
    } else {
      const int err = pthread_cond_timedwait(&cv_, &mu_, &abs_timeout);
      if (err == ETIMEDOUT) {
        --waiter_count_;
        return false;
      }
      if (err != 0) {
        ABSL_RAW_LOG(FATAL, "pthread_cond_timedwait failed: %d", err);
      }
    }
    first_pass = false;
  }
  // Consume a wakeup and we're done.
  --wakeup_count_;
  --waiter_count_;
  return true;
}

void PthreadWaiter::Post() {
  PthreadMutexHolder h(&mu_);
  ++wakeup_count_;
  InternalCondVarPoke();
}

void PthreadWaiter::Poke() {
  PthreadMutexHolder h(&mu_);
  InternalCondVarPoke();
}

void PthreadWaiter::InternalCondVarPoke() {
  if (waiter_count_ != 0) {
    const int err = pthread_cond_signal(&cv_);
    if (ABSL_PREDICT_FALSE(err != 0)) {
      ABSL_RAW_LOG(FATAL, "pthread_cond_signal failed: %d", err);
    }
  }
}

}  // namespace synchronization_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_INTERNAL_HAVE_PTHREAD_WAITER
