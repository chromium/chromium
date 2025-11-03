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
#include "base/types/expected_macros.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"
#include "ui/gl/gl_bindings.h"

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

GLsizeiptr GL_APIENTRY GLBlobCacheGetCallback(const void* key,
                                              GLsizeiptr key_size,
                                              void* value_out,
                                              GLsizeiptr value_size,
                                              const void* user_param) {
  DCHECK(user_param != nullptr);
  GpuPersistentCache* cache =
      static_cast<GpuPersistentCache*>(const_cast<void*>(user_param));

  return cache->GLBlobCacheGet(key, key_size, value_out, value_size);
}

void GL_APIENTRY GLBlobCacheSetCallback(const void* key,
                                        GLsizeiptr key_size,
                                        const void* value,
                                        GLsizeiptr value_size,
                                        const void* user_param) {
  DCHECK(user_param != nullptr);
  GpuPersistentCache* cache =
      static_cast<GpuPersistentCache*>(const_cast<void*>(user_param));

  cache->GLBlobCacheSet(key, key_size, value, value_size);
}

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
  std::unique_ptr<persistent_cache::Entry> entry = LoadImpl(key_str);
  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(value), value_size)));
  }

  return entry->GetContentSize();
}

sk_sp<SkData> GpuPersistentCache::load(const SkData& key) {
  std::string_view key_str(static_cast<const char*>(key.data()), key.size());
  std::unique_ptr<persistent_cache::Entry> entry = LoadImpl(key_str);
  if (!entry) {
    return nullptr;
  }

  sk_sp<SkData> output_data =
      SkData::MakeUninitialized(entry->GetContentSize());
  entry->CopyContentTo(UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(output_data->writable_data()),
                 output_data->size())));
  return output_data;
}

int64_t GpuPersistentCache::GLBlobCacheGet(const void* key,
                                           int64_t key_size,
                                           void* value_out,
                                           int64_t value_size) {
  CHECK_GE(key_size, 0);
  std::string_view key_str(static_cast<const char*>(key),
                           static_cast<size_t>(key_size));
  std::unique_ptr<persistent_cache::Entry> entry = LoadImpl(key_str);
  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(UNSAFE_BUFFERS(base::span(
        static_cast<uint8_t*>(value_out), static_cast<size_t>(value_size))));
  }

  return static_cast<GLsizeiptr>(entry->GetContentSize());
}

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::LoadEntry(
    std::string_view key) {
  return LoadImpl(key);
}

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::LoadImpl(
    std::string_view key) {
  ScopedHistogramTimer timer(GetHistogramName("Load"));
  const bool initialized = initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::LoadImpl", "persistent_cache_",
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

  ASSIGN_OR_RETURN(auto entry, persistent_cache_->Find(key),
                   [](persistent_cache::TransactionError error)
                       -> std::unique_ptr<persistent_cache::Entry> {
                     // TODO(crbug.com/377475540): Handle or at least address
                     // permanent errors.
                     return nullptr;
                   });

  return entry;
}

void GpuPersistentCache::StoreData(const void* key,
                                   size_t key_size,
                                   const void* value,
                                   size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(value), value_size));
  StoreImpl(key_str, value_span);
}

void GpuPersistentCache::store(const SkData& key, const SkData& data) {
  std::string_view key_str(static_cast<const char*>(key.data()), key.size());
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(data.bytes()), data.size()));
  StoreImpl(key_str, value_span);
}

void GpuPersistentCache::GLBlobCacheSet(const void* key,
                                        int64_t key_size,
                                        const void* value,
                                        int64_t value_size) {
  CHECK_GE(key_size, 0);
  CHECK_GE(value_size, 0);
  std::string_view key_str(static_cast<const char*>(key),
                           static_cast<size_t>(key_size));
  base::span<const uint8_t> value_span = UNSAFE_BUFFERS(base::span(
      static_cast<const uint8_t*>(value), static_cast<size_t>(value_size)));
  StoreImpl(key_str, value_span);
}

void GpuPersistentCache::StoreImpl(std::string_view key,
                                   base::span<const uint8_t> value) {
  ScopedHistogramTimer timer(GetHistogramName("Store"));
  const bool initialized = initialized_.IsSet();
  TRACE_EVENT1("gpu", "GpuPersistentCache::StoreImpl", "persistent_cache_",
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

  RETURN_IF_ERROR(persistent_cache_->Insert(key, value),
                  [](persistent_cache::TransactionError error) {
                    // TODO(crbug.com/377475540): Handle or at least address
                    // permanent errors.
                    return;
                  });
}

std::string GpuPersistentCache::GetHistogramName(
    std::string_view metric) const {
  return "GPU.PersistentCache." + cache_prefix_ + "." + std::string(metric);
}

void BindCacheToCurrentOpenGLContext(GpuPersistentCache* cache) {
  if (!cache || !gl::g_current_gl_driver->ext.b_GL_ANGLE_blob_cache) {
    return;
  }

  glBlobCacheCallbacksANGLE(GLBlobCacheSetCallback, GLBlobCacheGetCallback,
                            cache);
}

void UnbindCacheFromCurrentOpenGLContext() {
  if (!gl::g_current_gl_driver->ext.b_GL_ANGLE_blob_cache) {
    return;
  }

  glBlobCacheCallbacksANGLE(nullptr, nullptr, nullptr);
}

}  // namespace gpu
