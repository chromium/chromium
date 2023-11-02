// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampling_thread_win.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"

namespace tracing {

namespace {

// This is a prime number close to the stack sampling interval defined in
// tracing_sampler_profiler.cc, so that loader lock sampling happens on a
// similar frequency to stack sampling but not at identical times to reduce the
// change of the measurements interfering.
constexpr int kSamplingIntervalMsec = 53;

LoaderLockSampler* g_loader_lock_sampler = nullptr;

ProbingLoaderLockSampler* DefaultLoaderLockSampler() {
  static base::NoDestructor<ProbingLoaderLockSampler> default_sampler;
  return default_sampler.get();
}

}  // namespace

const char LoaderLockSamplingThread::kLoaderLockHeldEventName[] =
    "LoaderLockHeld (sampled)";

LoaderLockSamplingThread::LoaderLockSamplingThread()
    : base::Thread("LoaderLockSampler") {
  // The loader lock sampler may already have been set if
  // SetLoaderLockSamplerForTesting was called.
  if (!g_loader_lock_sampler)
    g_loader_lock_sampler = DefaultLoaderLockSampler();
  Start();
  DCHECK(task_runner());  // This should exist after Start is called.
  tracker_ = base::SequenceBound<LoaderLockTracker>(task_runner());
}

LoaderLockSamplingThread::~LoaderLockSamplingThread() {
  Stop();
}

// static
void LoaderLockSamplingThread::SetLoaderLockSamplerForTesting(
    LoaderLockSampler* sampler) {
  g_loader_lock_sampler = sampler ? sampler : DefaultLoaderLockSampler();
}

void LoaderLockSamplingThread::StartSampling() {
  tracker_.AsyncCall(&LoaderLockTracker::StartSampling);
}

void LoaderLockSamplingThread::StopSampling() {
  tracker_.AsyncCall(&LoaderLockTracker::StopSampling);
}

void LoaderLockSamplingThread::LoaderLockTracker::StartSampling() {
  sample_timer_.Start(FROM_HERE, base::Milliseconds(kSamplingIntervalMsec),
                      this, &LoaderLockTracker::SampleLoaderLock);
}

void LoaderLockSamplingThread::LoaderLockTracker::StopSampling() {
  sample_timer_.Stop();
}

void LoaderLockSamplingThread::LoaderLockTracker::SampleLoaderLock() {
  DCHECK(g_loader_lock_sampler);
  bool loader_lock_now_held = g_loader_lock_sampler->IsLoaderLockHeld();

  if (loader_lock_now_held && !loader_lock_is_held_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                                      kLoaderLockHeldEventName,
                                      TRACE_ID_LOCAL(this));
  } else if (!loader_lock_now_held && loader_lock_is_held_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                                    kLoaderLockHeldEventName,
                                    TRACE_ID_LOCAL(this));
  }
  loader_lock_is_held_ = loader_lock_now_held;
}

}  // namespace tracing
