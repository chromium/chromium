// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/passthrough_program_cache.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {
namespace gles2 {

namespace {

bool BlobCacheExtensionAvailable(gl::GLDisplayEGL* gl_display) {
  // The display should be initialized if the extension is available.
  return gl_display->ext->b_EGL_ANDROID_blob_cache;
}

// EGL_ANDROID_blob_cache doesn't give user pointer to the callbacks so we are
// forced to have this be global.
PassthroughProgramCache* g_program_cache = nullptr;

// Blob cache function pointers can only be set once per EGLDisplay. Using a
// single bool is not technically correct, but it's acceptable since we only
// have one EGLDisplay in practice.
bool g_blob_cache_funcs_set = false;

}  // namespace

PassthroughProgramCache::PassthroughProgramCache(
    size_t max_cache_size_bytes,
    bool disable_gpu_shader_disk_cache,
    ValueAddedHook* value_added_hook)
    : ProgramCache(max_cache_size_bytes),
      disable_gpu_shader_disk_cache_(disable_gpu_shader_disk_cache),
      curr_size_bytes_(0),
      store_(ProgramLRUCache::NO_AUTO_EVICT),
      value_added_hook_(value_added_hook) {
  gl::GLDisplayEGL* gl_display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  EGLDisplay egl_display = gl_display->GetDisplay();

  DCHECK(!g_program_cache);
  g_program_cache = this;

  // display is EGL_NO_DISPLAY during unittests.
  if (egl_display != EGL_NO_DISPLAY && !g_blob_cache_funcs_set &&
      BlobCacheExtensionAvailable(gl_display)) {
    // Register the blob cache callbacks.
    eglSetBlobCacheFuncsANDROID(egl_display, BlobCacheSet, BlobCacheGet);
    g_blob_cache_funcs_set = true;
  }
}

PassthroughProgramCache::~PassthroughProgramCache() {
  // Clear up the blob cache callbacks.  Note that this not allowed by the
  // EGL_ANDROID_blob_cache spec, so we just set the pointer to this object to
  // nullptr as a workaround.  The callbacks don't work with this pointer
  // missing.
  g_program_cache = nullptr;
}

void PassthroughProgramCache::ClearBackend() {
  base::AutoLock auto_lock(lock_);
  store_.Clear();
  DCHECK_EQ(0U, curr_size_bytes_);
}

ProgramCache::ProgramLoadResult PassthroughProgramCache::LoadLinkedProgram(
    GLuint program,
    Shader* shader_a,
    Shader* shader_b,
    const LocationMap* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode,
    DecoderClient* client) {
  NOTREACHED_IN_MIGRATION();
  return PROGRAM_LOAD_FAILURE;
}

void PassthroughProgramCache::SaveLinkedProgram(
    GLuint program,
    const Shader* shader_a,
    const Shader* shader_b,
    const LocationMap* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode,
    DecoderClient* client) {
  NOTREACHED_IN_MIGRATION();
}

void PassthroughProgramCache::LoadProgram(const std::string& key,
                                          const std::string& program) {
  base::AutoLock auto_lock(lock_);
  if (!CacheEnabled()) {
    return;
  }

  std::string key_decoded;
  std::string program_decoded;
  base::Base64Decode(key, &key_decoded);
  base::Base64Decode(program, &program_decoded);

  Key entry_key(key_decoded.begin(), key_decoded.end());
  Value entry_value(program_decoded.begin(), program_decoded.end());

  store_.Put(entry_key, ProgramCacheValue(std::move(entry_value), this));
}

size_t PassthroughProgramCache::Trim(size_t limit) {
  base::AutoLock auto_lock(lock_);
  size_t initial_size = curr_size_bytes_;
  while (curr_size_bytes_ > limit) {
    DCHECK(!store_.empty());
    store_.Erase(store_.rbegin());
  }
  return initial_size - curr_size_bytes_;
}

bool PassthroughProgramCache::CacheEnabled() const {
  return !disable_gpu_shader_disk_cache_;
}

void PassthroughProgramCache::Set(Key&& key, Value&& value) {
  {
    base::AutoLock auto_lock(lock_);
    // If the value is so big it will never fit in the cache, throw it away.
    if (value.size() > max_size_bytes()) {
      return;
    }

    // Evict any cached program with the same key in favor of the least recently
    // accessed.
    ProgramLRUCache::iterator existing = store_.Peek(key);
    if (existing != store_.end()) {
      store_.Erase(existing);
    }

    // If the cache is overflowing, remove some old entries.
    DCHECK(max_size_bytes() >= value.size());
  }
  Trim(max_size_bytes() - value.size());
  {
    base::AutoLock auto_lock(lock_);
    // If callback is set, notify that there was a new/updated blob entry so it
    // can be stored in disk.  Note that this is done before the Put() call as
    // that consumes `value`.
    if (cache_program_callback_) {
      // Convert the key and binary to string form.
      std::string_view key_string(reinterpret_cast<const char*>(key.data()),
                                  key.size());
      std::string_view value_string(reinterpret_cast<const char*>(value.data()),
                                    value.size());
      std::string key_string_64 = base::Base64Encode(key_string);
      std::string value_string_64 = base::Base64Encode(value_string);
      cache_program_callback_.Run(key_string_64, value_string_64);
    }

    if (value_added_hook_) {
      value_added_hook_->OnValueAddedToCache(key, value);
    }

    store_.Put(key, ProgramCacheValue(std::move(value), this));
  }
}

const PassthroughProgramCache::ProgramCacheValue* PassthroughProgramCache::Get(
    const Key& key) {
  lock_.AssertAcquired();
  ProgramLRUCache::iterator found = store_.Get(key);
  return found == store_.end() ? nullptr : &found->second;
}

EGLsizeiANDROID PassthroughProgramCache::BlobCacheGetImpl(
    const void* key,
    EGLsizeiANDROID key_size,
    void* value,
    EGLsizeiANDROID value_size) {
  if (key_size < 0) {
    return 0;
  }

  const uint8_t* key_begin = reinterpret_cast<const uint8_t*>(key);
  PassthroughProgramCache::Key entry_key(key_begin, key_begin + key_size);

  // Note that the |lock_| should be held during whole time ProgramCacheValue is
  // being accessed below.
  base::AutoLock auto_lock(lock_);
  const PassthroughProgramCache::ProgramCacheValue* cache_value =
      g_program_cache->Get(std::move(entry_key));

  UMA_HISTOGRAM_BOOLEAN("Gpu.PassthroughProgramCacheLoadHitInCache",
                        cache_value != nullptr);

  if (!cache_value) {
    return 0;
  }

  const PassthroughProgramCache::Value& entry_value = cache_value->data();

  if (value_size > 0) {
    if (static_cast<size_t>(value_size) >= entry_value.size()) {
      memcpy(value, entry_value.data(), entry_value.size());
    }
  }

  return entry_value.size();
}

void PassthroughProgramCache::BlobCacheSet(const void* key,
                                           EGLsizeiANDROID key_size,
                                           const void* value,
                                           EGLsizeiANDROID value_size) {
  if (!g_program_cache)
    return;

  if (key_size < 0 || value_size < 0)
    return;

  const uint8_t* key_begin = reinterpret_cast<const uint8_t*>(key);
  PassthroughProgramCache::Key entry_key(key_begin, key_begin + key_size);

  const uint8_t* value_begin = reinterpret_cast<const uint8_t*>(value);
  PassthroughProgramCache::Value entry_value(value_begin,
                                             value_begin + value_size);

  g_program_cache->Set(std::move(entry_key), std::move(entry_value));
}

EGLsizeiANDROID PassthroughProgramCache::BlobCacheGet(
    const void* key,
    EGLsizeiANDROID key_size,
    void* value,
    EGLsizeiANDROID value_size) {
  if (!g_program_cache)
    return 0;

  return g_program_cache->BlobCacheGetImpl(key, key_size, value, value_size);
}

PassthroughProgramCache::ProgramCacheValue::ProgramCacheValue(
    PassthroughProgramCache::Value&& program_blob,
    PassthroughProgramCache* program_cache)
    : program_blob_(std::move(program_blob)), program_cache_(program_cache) {
  program_cache_->curr_size_bytes_ += program_blob_.size();
}

PassthroughProgramCache::ProgramCacheValue::~ProgramCacheValue() {
  if (program_cache_) {
    program_cache_->curr_size_bytes_ -= program_blob_.size();
  }
}

PassthroughProgramCache::ProgramCacheValue::ProgramCacheValue(
    ProgramCacheValue&& other)
    : program_blob_(std::move(other.program_blob_)),
      program_cache_(std::exchange(other.program_cache_, nullptr)) {}

PassthroughProgramCache::ProgramCacheValue&
PassthroughProgramCache::ProgramCacheValue::operator=(
    ProgramCacheValue&& other) {
  program_blob_ = std::move(other.program_blob_);
  program_cache_ = std::exchange(other.program_cache_, nullptr);
  return *this;
}

}  // namespace gles2
}  // namespace gpu
