// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/containers/span_writer.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "third_party/angle/src/common/angle_version_info.h"

namespace gpu {
namespace gles2 {

ProgramCache::ScopedCacheUse::ScopedCacheUse(ProgramCache* cache,
                                             CacheProgramCallback callback)
    : cache_(cache) {
  base::AutoLock auto_lock(cache_->lock_);
  // The existing callback should be null, otherwise we'll overwrite it.
  DCHECK(!cache_->cache_program_callback_);
  cache_->cache_program_callback_ = std::move(callback);
}

ProgramCache::ScopedCacheUse::~ScopedCacheUse() {
  base::AutoLock auto_lock(cache_->lock_);
  // The callback should be the one installed by the constructor. The DCHECK
  // doesn't exactly check that, but checking for non-null is a cheap second.
  DCHECK(cache_->cache_program_callback_);
  cache_->cache_program_callback_.Reset();
}

ProgramCache::ProgramCache(size_t max_cache_size_bytes)
    : max_size_bytes_(max_cache_size_bytes) {}
ProgramCache::~ProgramCache() = default;

void ProgramCache::Clear() {
  ClearBackend();
  link_status_.clear();
  compiled_shaders_.clear();
}

bool ProgramCache::HasSuccessfullyCompiledShader(
    const std::string& shader_signature) const {
  Hash sha;
  ComputeShaderHash(shader_signature, sha);
  return base::Contains(compiled_shaders_, sha);
}

ProgramCache::LinkedProgramStatus ProgramCache::GetLinkedProgramStatus(
    const std::string& shader_signature_a,
    const std::string& shader_signature_b,
    const std::map<std::string, GLint>* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode) const {
  Hash a_sha;
  Hash b_sha;
  ComputeShaderHash(shader_signature_a, a_sha);
  ComputeShaderHash(shader_signature_b, b_sha);

  Hash program_sha;
  ComputeProgramHash(a_sha, b_sha, bind_attrib_location_map,
                     transform_feedback_varyings,
                     transform_feedback_buffer_mode, program_sha);
  auto found_it = link_status_.find(program_sha);
  if (found_it == link_status_.end()) {
    return ProgramCache::LINK_UNKNOWN;
  } else {
    return found_it->second;
  }
}

void ProgramCache::LinkedProgramCacheSuccess(
    const std::string& shader_signature_a,
    const std::string& shader_signature_b,
    const LocationMap* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode) {
  Hash a_sha;
  Hash b_sha;
  ComputeShaderHash(shader_signature_a, a_sha);
  ComputeShaderHash(shader_signature_b, b_sha);
  Hash program_sha;
  ComputeProgramHash(a_sha, b_sha, bind_attrib_location_map,
                     transform_feedback_varyings,
                     transform_feedback_buffer_mode, program_sha);
  CompiledShaderCacheSuccess(a_sha);
  CompiledShaderCacheSuccess(b_sha);
  LinkedProgramCacheSuccess(program_sha);
}

void ProgramCache::LinkedProgramCacheSuccess(const Hash& program_hash) {
  link_status_[program_hash] = LINK_SUCCEEDED;
}

void ProgramCache::CompiledShaderCacheSuccess(const Hash& shader_hash) {
  compiled_shaders_.insert(shader_hash);
}

void ProgramCache::ComputeShaderHash(std::string_view str, Hash& result) const {
  result = base::SHA1Hash(base::as_byte_span(str));
}

void ProgramCache::Evict(const Hash& program_hash,
                         const Hash& shader_0_hash,
                         const Hash& shader_1_hash) {
  link_status_.erase(program_hash);
  compiled_shaders_.erase(shader_0_hash);
  compiled_shaders_.erase(shader_1_hash);
}

namespace {
size_t CalculateMapSize(const std::map<std::string, GLint>* map) {
  if (!map) {
    return 0;
  }
  size_t total = 0;
  for (auto it = map->begin(); it != map->end(); ++it) {
    total += 4 + it->first.length();
  }
  return total;
}

size_t CalculateVaryingsSize(const std::vector<std::string>& varyings) {
  size_t total = 0;
  for (auto& varying : varyings) {
    total += 1 + varying.length();
  }
  return total;
}
}  // anonymous namespace

void ProgramCache::ComputeProgramHash(
    HashView hashed_shader_0,
    HashView hashed_shader_1,
    const std::map<std::string, GLint>* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode,
    Hash& result) const {
  const size_t shader0_size = hashed_shader_0.size();
  const size_t shader1_size = hashed_shader_1.size();
  const size_t angle_commit_size = angle::GetANGLECommitHashSize();
  const size_t map_size = CalculateMapSize(bind_attrib_location_map);
  const size_t var_size = CalculateVaryingsSize(transform_feedback_varyings);
  const size_t total_size = shader0_size + shader1_size + angle_commit_size
      + map_size + var_size + sizeof(transform_feedback_buffer_mode);

  auto buffer_storage = base::HeapArray<uint8_t>::Uninit(total_size);
  auto buffer = base::SpanWriter(base::span(buffer_storage));

  CHECK(buffer.Write(hashed_shader_0));
  CHECK(buffer.Write(hashed_shader_1));
  CHECK(buffer.Write(base::as_byte_span(
      // SAFETY: angle::GetANGLECommitHashSize() gives the number of bytes
      // pointed to by angle::GetANGLECommitHash().
      UNSAFE_BUFFERS(
          base::span(angle::GetANGLECommitHash(), angle_commit_size)))));

  if (map_size != 0) {
    // copy our map
    for (auto it = bind_attrib_location_map->begin();
         it != bind_attrib_location_map->end();
         ++it) {
      CHECK(buffer.Write(base::as_byte_span(it->first)));
      CHECK(buffer.WriteI32BigEndian(it->second));
    }
  }

  if (var_size != 0) {
    // copy transform feedback varyings
    for (auto& varying : transform_feedback_varyings) {
      CHECK(buffer.Write(base::as_byte_span(varying)));
      CHECK(buffer.WriteU8LittleEndian(uint8_t{' '}));
    }
  }
  CHECK(buffer.Write(base::byte_span_from_ref(transform_feedback_buffer_mode)));
  CHECK_EQ(buffer.remaining(), 0u);  // Verify the size was computed correctly.
  result = base::SHA1Hash(buffer_storage);
}

void ProgramCache::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // Set a low limit on cache size for MEMORY_PRESSURE_LEVEL_MODERATE.
  size_t limit = max_size_bytes_ / 4;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    limit = 0;
  }

  Trim(limit);
}

size_t ProgramCache::HashHasher::operator()(const Hash& hash) const {
  return base::FastHash(hash);
}

}  // namespace gles2
}  // namespace gpu
