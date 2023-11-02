// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_WIN_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_WIN_H_

#include "base/component_export.h"
#include "base/threading/thread_checker.h"

namespace tracing {

// This class can be implemented to check whether the loader lock is held.
class COMPONENT_EXPORT(TRACING_CPP) LoaderLockSampler {
 public:
  virtual ~LoaderLockSampler() = default;

  // Checks if the loader lock is currently held by another thread. Returns
  // false if the lock is already held by the calling thread since the lock can
  // be taken recursively.
  virtual bool IsLoaderLockHeld() const = 0;
};

// Implementation of LoaderLockSampler that probes the loader lock by
// attempting to take it, and immediately drops it if successful.
class COMPONENT_EXPORT(TRACING_CPP) ProbingLoaderLockSampler final
    : public LoaderLockSampler {
 public:
  ProbingLoaderLockSampler();
  ~ProbingLoaderLockSampler() final;

  bool IsLoaderLockHeld() const final;

 private:
  // Ensures that the loader lock is only probed from a single OS thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_WIN_H_
