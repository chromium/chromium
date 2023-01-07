// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLING_THREAD_WIN_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLING_THREAD_WIN_H_

#include "base/component_export.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"

namespace tracing {

class LoaderLockSampler;

// A thread that periodically samples the Windows loader lock to see if it is
// being held. This is a dedicated thread, instead of using the ThreadPool, so
// that there's never a chance of the lock being sampled from a thread that
// runs other code that takes the lock.
class COMPONENT_EXPORT(TRACING_CPP) LoaderLockSamplingThread final
    : public base::Thread {
 public:
  LoaderLockSamplingThread();
  ~LoaderLockSamplingThread() override;

  LoaderLockSamplingThread(const LoaderLockSamplingThread& other) = delete;
  LoaderLockSamplingThread& operator=(const LoaderLockSamplingThread& other) =
      delete;

  // The name of a trace event that will be recorded when the loader lock is
  // held. Exposed for testing.
  static const char kLoaderLockHeldEventName[];

  // Registers a mock LoaderLockSampler to be called during tests. |sampler| is
  // owned by the caller. It must be reset to |nullptr| at the end of the test,
  // which will cause the default ProbingLoaderLockSampler to be used.
  static void SetLoaderLockSamplerForTesting(LoaderLockSampler* sampler);

  // Begins periodic sampling. This can be called from any thread, and the
  // sampling will be performed on the LoaderLockSamplingThread.
  void StartSampling();

  // Stops periodic sampling. This can be called from any thread.
  void StopSampling();

 private:
  // Class that performs the sampling on the sampling thread.
  class LoaderLockTracker {
   public:
    LoaderLockTracker() = default;
    ~LoaderLockTracker() = default;

    LoaderLockTracker(const LoaderLockTracker& other) = delete;
    LoaderLockTracker& operator=(const LoaderLockTracker& other) = delete;

    void StartSampling();
    void StopSampling();

   private:
    void SampleLoaderLock();

    base::RepeatingTimer sample_timer_;
    bool loader_lock_is_held_ = false;
  };

  base::SequenceBound<LoaderLockTracker> tracker_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLING_THREAD_WIN_H_
