// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_platform.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/config/gpu_finch_features.h"

namespace gpu::webgpu {

namespace {

class AsyncWaitableEventImpl
    : public base::RefCountedThreadSafe<AsyncWaitableEventImpl> {
 public:
  explicit AsyncWaitableEventImpl()
      : waitable_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void Wait() { waitable_event_.Wait(); }

  bool IsComplete() { return waitable_event_.IsSignaled(); }

  void MarkAsComplete() { waitable_event_.Signal(); }

 private:
  friend class base::RefCountedThreadSafe<AsyncWaitableEventImpl>;
  ~AsyncWaitableEventImpl() = default;

  base::WaitableEvent waitable_event_;
};

class AsyncWaitableEvent : public dawn::platform::WaitableEvent {
 public:
  explicit AsyncWaitableEvent()
      : waitable_event_impl_(base::MakeRefCounted<AsyncWaitableEventImpl>()) {}

  void Wait() override { waitable_event_impl_->Wait(); }

  bool IsComplete() override { return waitable_event_impl_->IsComplete(); }

  scoped_refptr<AsyncWaitableEventImpl> GetWaitableEventImpl() const {
    return waitable_event_impl_;
  }

 private:
  scoped_refptr<AsyncWaitableEventImpl> waitable_event_impl_;
};

class AsyncWorkerTaskPool : public dawn::platform::WorkerTaskPool {
 public:
  std::unique_ptr<dawn::platform::WaitableEvent> PostWorkerTask(
      dawn::platform::PostWorkerTaskCallback callback,
      void* user_data) override {
    std::unique_ptr<AsyncWaitableEvent> waitable_event =
        std::make_unique<AsyncWaitableEvent>();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&RunWorkerTask, callback, user_data,
                       waitable_event->GetWaitableEventImpl()));
    return waitable_event;
  }

 private:
  static void RunWorkerTask(
      dawn::platform::PostWorkerTaskCallback callback,
      void* user_data,
      scoped_refptr<AsyncWaitableEventImpl> waitable_event_impl) {
    TRACE_EVENT0("toplevel", "DawnPlatformImpl::RunWorkerTask");
    callback(user_data);
    waitable_event_impl->MarkAsComplete();
  }
};

void RecordDelayedUMA(scoped_refptr<DawnPlatform::CacheCountsMap> cache_map,
                      std::string uma_prefix) {
  base::AutoLock autolock(cache_map->lock);
  for (auto [uma_name, cache_counts] : cache_map->counts) {
    if (uma_name.find("Hit") != std::string::npos) {
      base::UmaHistogramCounts10000(
          uma_prefix + uma_name + ".Counts.90SecondsPostStartup",
          cache_counts.cache_hit_count);
    } else {
      CHECK(uma_name.find("Miss") != std::string::npos);
      base::UmaHistogramCounts10000(
          uma_prefix + uma_name + ".Counts.90SecondsPostStartup",
          cache_counts.cache_miss_count);
    }
  }
}

}  // anonymous namespace

DawnPlatform::CacheCountsMap::CacheCountsMap() = default;
DawnPlatform::CacheCountsMap::~CacheCountsMap() = default;

DawnPlatform::DawnPlatform(
    std::unique_ptr<DawnCachingInterface> dawn_caching_interface,
    const char* uma_prefix,
    bool record_cache_count_uma)
    : dawn_caching_interface_(std::move(dawn_caching_interface)),
      uma_prefix_(uma_prefix),
      cache_map_(base::MakeRefCounted<CacheCountsMap>()),
      startup_time_(base::TimeTicks::Now()) {
  if (record_cache_count_uma) {
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordDelayedUMA, cache_map_, uma_prefix_),
        base::Seconds(90));
  }
}

DawnPlatform::~DawnPlatform() = default;

const unsigned char* DawnPlatform::GetTraceCategoryEnabledFlag(
    dawn::platform::TraceCategory category) {
  // For now, all Dawn trace categories are put under "gpu.dawn"
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("gpu.dawn"));
}

double DawnPlatform::MonotonicallyIncreasingTime() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
}

uint64_t DawnPlatform::AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    uint64_t id,
    double timestamp,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const uint64_t* arg_values,
    unsigned char flags) {
  base::TimeTicks timestamp_tt = base::TimeTicks() + base::Seconds(timestamp);

  base::trace_event::TraceArguments args(
      num_args, arg_names, arg_types,
      reinterpret_cast<const unsigned long long*>(arg_values));

  base::trace_event::TraceEventHandle handle =
      TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
          phase, category_group_enabled, name,
          trace_event_internal::kGlobalScope, id, trace_event_internal::kNoId,
          base::PlatformThread::CurrentId(), timestamp_tt, &args, flags);

  uint64_t result = 0;
  static_assert(sizeof(base::trace_event::TraceEventHandle) <= sizeof(result),
                "TraceEventHandle must be at most the size of uint64_t");
  static_assert(
      std::is_trivial_v<base::trace_event::TraceEventHandle> &&
          std::is_standard_layout_v<base::trace_event::TraceEventHandle>,
      "TraceEventHandle must be memcpy'able");
  memcpy(&result, &handle, sizeof(base::trace_event::TraceEventHandle));
  return result;
}

void DawnPlatform::HistogramCacheCountHelper(std::string name,
                                             int sample,
                                             int min,
                                             int max,
                                             int bucketCount) {
  if (name.find("Cache") != std::string::npos) {
    base::AutoLock autolock(cache_map_->lock);
    auto& cache_counts = cache_map_->counts[name];
    if (name.find("Hit") != std::string::npos) {
      ++cache_counts.cache_hit_count;
    } else {
      CHECK(name.find("Miss") != std::string::npos);
      ++cache_counts.cache_miss_count;
    }

    if (base::TimeTicks::Now() - startup_time_ <= base::Seconds(90)) {
      base::UmaHistogramCustomCounts(
          uma_prefix_ + name + ".90SecondsPostStartup", sample, min, max,
          bucketCount);
    }
  }
}

void DawnPlatform::HistogramCustomCounts(const char* name,
                                         int sample,
                                         int min,
                                         int max,
                                         int bucketCount) {
  base::UmaHistogramCustomCounts(uma_prefix_ + name, sample, min, max,
                                 bucketCount);
  HistogramCacheCountHelper(name, sample, min, max, bucketCount);
}

void DawnPlatform::HistogramCustomCountsHPC(const char* name,
                                            int sample,
                                            int min,
                                            int max,
                                            int bucketCount) {
  if (base::TimeTicks::IsHighResolution()) {
    base::UmaHistogramCustomCounts(uma_prefix_ + name, sample, min, max,
                                   bucketCount);
    HistogramCacheCountHelper(name, sample, min, max, bucketCount);
  }
}

void DawnPlatform::HistogramEnumeration(const char* name,
                                        int sample,
                                        int boundaryValue) {
  base::UmaHistogramExactLinear(uma_prefix_ + name, sample, boundaryValue);
}

void DawnPlatform::HistogramSparse(const char* name, int sample) {
  base::UmaHistogramSparse(uma_prefix_ + name, sample);
}

void DawnPlatform::HistogramBoolean(const char* name, bool sample) {
  base::UmaHistogramBoolean(uma_prefix_ + name, sample);
}

dawn::platform::CachingInterface* DawnPlatform::GetCachingInterface() {
  return dawn_caching_interface_.get();
}

std::unique_ptr<dawn::platform::WorkerTaskPool>
DawnPlatform::CreateWorkerTaskPool() {
  return std::make_unique<AsyncWorkerTaskPool>();
}

bool DawnPlatform::IsFeatureEnabled(dawn::platform::Features feature) {
  switch (feature) {
    case dawn::platform::Features::kWebGPUUseDXC:
      return base::FeatureList::IsEnabled(features::kWebGPUUseDXC);
    case dawn::platform::Features::kWebGPUUseTintIR:
      return base::FeatureList::IsEnabled(features::kWebGPUUseTintIR);
    default:
      return false;
  }
}

}  // namespace gpu::webgpu
