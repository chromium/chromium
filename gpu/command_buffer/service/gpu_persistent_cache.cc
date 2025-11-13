// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"

namespace gpu {

namespace {

constexpr size_t kMaxLoadStoreForTrackingCacheAvailable = 100;

class ScopedHistogramTimer {
 public:
  explicit ScopedHistogramTimer(const std::string& name) : name_(name) {}
  ~ScopedHistogramTimer() {
    if (enabled_) {
      base::UmaHistogramCustomMicrosecondsTimes(name_, timer_.Elapsed(),
                                                base::Microseconds(1),
                                                base::Seconds(30), 100);
    }
  }

  void SetEnabled(bool enabled) { enabled_ = enabled; }

 private:
  const std::string name_;
  base::ElapsedTimer timer_;
  bool enabled_ = true;
};

}  // namespace

GpuPersistentCache::GpuPersistentCache(std::string_view cache_prefix)
    : cache_prefix_(cache_prefix) {}

GpuPersistentCache::~GpuPersistentCache() = default;

void GpuPersistentCache::InitializeCache(
    persistent_cache::BackendParams backend_params) {
  CHECK(!initialized_.IsSet());
  persistent_cache_ =
      persistent_cache::PersistentCache::Open(std::move(backend_params));
  if (persistent_cache_) {
    initialized_.Set();
  }
}

size_t GpuPersistentCache::LoadData(const void* key,
                                    size_t key_size,
                                    void* value,
                                    size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  std::unique_ptr<persistent_cache::Entry> entry = LoadEntry(key_str);
  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(
        UNSAFE_TODO(base::span(static_cast<uint8_t*>(value), value_size)));
  }

  return entry->GetContentSize();
}

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::LoadEntry(
    std::string_view key) {
  ScopedHistogramTimer timer(GetHistogramName("Load"));
  const bool initialized = initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::LoadEntry", "persistent_cache_",
               initialized);

  // Track cache available for the 1st kMaxLoadStoreForTrackingCacheAvailable
  // loads.
  if (load_count_.fetch_add(1, std::memory_order_relaxed) <
      kMaxLoadStoreForTrackingCacheAvailable) {
    base::UmaHistogramBoolean(GetHistogramName("Load.CacheAvailable"),
                              initialized);
  }

  if (!initialized) {
    timer.SetEnabled(false);
    return nullptr;
  }

  return persistent_cache_->Find(key);

}

void GpuPersistentCache::StoreData(const void* key,
                                   size_t key_size,
                                   const void* value,
                                   size_t value_size) {
  ScopedHistogramTimer timer(GetHistogramName("Store"));
  const bool initialized = initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::StoreData", "persistent_cache_",
               initialized);

  // Track cache available for the 1st kMaxLoadStoreForTrackingCacheAvailable
  // stores.
  if (store_count_.fetch_add(1, std::memory_order_relaxed) <
      kMaxLoadStoreForTrackingCacheAvailable) {
    base::UmaHistogramBoolean(GetHistogramName("Store.CacheAvailable"),
                              initialized);
  }

  if (!initialized) {
    timer.SetEnabled(false);
    return;
  }

  std::string_view key_str(static_cast<const char*>(key), key_size);
  persistent_cache_->Insert(
      key_str,
      UNSAFE_TODO(base::span(static_cast<const uint8_t*>(value), value_size)));
}

std::string GpuPersistentCache::GetHistogramName(
    std::string_view metric) const {
  return "GPU.PersistentCache." + cache_prefix_ + "." + std::string(metric);
}

}  // namespace gpu
