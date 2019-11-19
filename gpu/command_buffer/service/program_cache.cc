// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "third_party/angle/src/common/version.h"

namespace gpu {
namespace gles2 {

ProgramCache::ScopedCacheUse::ScopedCacheUse(ProgramCache* cache,
                                             CacheProgramCallback callback)
    : cache_(cache) {
  cache_->cache_program_callback_ = callback;
}

ProgramCache::ScopedCacheUse::~ScopedCacheUse() {
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
  char sha[kHashLength];
  ComputeShaderHash(shader_signature, sha);
  const std::string sha_string(sha, kHashLength);

  if (compiled_shaders_.find(sha_string) != compiled_shaders_.end()) {
    return true;
  }
  return false;
}

ProgramCache::LinkedProgramStatus ProgramCache::GetLinkedProgramStatus(
    const std::string& shader_signature_a,
    const std::string& shader_signature_b,
    const std::map<std::string, GLint>* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode) const {
  char a_sha[kHashLength];
  char b_sha[kHashLength];
  ComputeShaderHash(shader_signature_a, a_sha);
  ComputeShaderHash(shader_signature_b, b_sha);

  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     transform_feedback_varyings,
                     transform_feedback_buffer_mode,
                     sha);
  const std::string sha_string(sha, kHashLength);

  LinkStatusMap::const_iterator found = link_status_.find(sha_string);
  if (found == link_status_.end()) {
    return ProgramCache::LINK_UNKNOWN;
  } else {
    return found->second;
  }
}

void ProgramCache::LinkedProgramCacheSuccess(
    const std::string& shader_signature_a,
    const std::string& shader_signature_b,
    const LocationMap* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode) {
  char a_sha[kHashLength];
  char b_sha[kHashLength];
  ComputeShaderHash(shader_signature_a, a_sha);
  ComputeShaderHash(shader_signature_b, b_sha);
  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     transform_feedback_varyings,
                     transform_feedback_buffer_mode,
                     sha);
  const std::string a_sha_string(a_sha, kHashLength);
  const std::string b_sha_string(b_sha, kHashLength);
  const std::string sha_string(sha, kHashLength);

  CompiledShaderCacheSuccess(a_sha_string);
  CompiledShaderCacheSuccess(b_sha_string);
  LinkedProgramCacheSuccess(sha_string);
}

void ProgramCache::LinkedProgramCacheSuccess(const std::string& program_hash) {
  link_status_[program_hash] = LINK_SUCCEEDED;
}

void ProgramCache::CompiledShaderCacheSuccess(const std::string& shader_hash) {
  compiled_shaders_.insert(shader_hash);
}

void ProgramCache::ComputeShaderHash(
    const std::string& str,
    char* result) const {
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(str.c_str()),
                      str.length(), reinterpret_cast<unsigned char*>(result));
}

void ProgramCache::Evict(const std::string& program_hash,
                         const std::string& shader_0_hash,
                         const std::string& shader_1_hash) {
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
    const char* hashed_shader_0,
    const char* hashed_shader_1,
    const std::map<std::string, GLint>* bind_attrib_location_map,
    const std::vector<std::string>& transform_feedback_varyings,
    GLenum transform_feedback_buffer_mode,
    char* result) const {
  const size_t shader0_size = kHashLength;
  const size_t shader1_size = kHashLength;
  const size_t angle_commit_size = ANGLE_COMMIT_HASH_SIZE;
  const size_t map_size = CalculateMapSize(bind_attrib_location_map);
  const size_t var_size = CalculateVaryingsSize(transform_feedback_varyings);
  const size_t total_size = shader0_size + shader1_size + angle_commit_size
      + map_size + var_size + sizeof(transform_feedback_buffer_mode);

  std::unique_ptr<unsigned char[]> buffer(new unsigned char[total_size]);
  memcpy(buffer.get(), hashed_shader_0, shader0_size);
  memcpy(&buffer[shader0_size], hashed_shader_1, shader1_size);
  size_t current_pos = shader0_size + shader1_size;
  memcpy(&buffer[current_pos], ANGLE_COMMIT_HASH, angle_commit_size);
  current_pos += angle_commit_size;
  if (map_size != 0) {
    // copy our map
    for (auto it = bind_attrib_location_map->begin();
         it != bind_attrib_location_map->end();
         ++it) {
      const size_t name_size = it->first.length();
      memcpy(&buffer.get()[current_pos], it->first.c_str(), name_size);
      current_pos += name_size;
      const GLint value = it->second;
      buffer[current_pos++] = value >> 24;
      buffer[current_pos++] = static_cast<unsigned char>(value >> 16);
      buffer[current_pos++] = static_cast<unsigned char>(value >> 8);
      buffer[current_pos++] = static_cast<unsigned char>(value);
    }
  }

  if (var_size != 0) {
    // copy transform feedback varyings
    for (auto& varying : transform_feedback_varyings) {
      const size_t name_size = varying.length();
      memcpy(&buffer.get()[current_pos], varying.c_str(), name_size);
      current_pos += name_size;
      buffer[current_pos++] = ' ';
    }
  }
  memcpy(&buffer[current_pos], &transform_feedback_buffer_mode,
      sizeof(transform_feedback_buffer_mode));
  base::SHA1HashBytes(buffer.get(),
                      total_size, reinterpret_cast<unsigned char*>(result));
}

void ProgramCache::HandleMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // This is only called with moderate or critical pressure.
  DCHECK_NE(memory_pressure_level,
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  // Set a low limit on cache size for MEMORY_PRESSURE_LEVEL_MODERATE.
  size_t limit = max_size_bytes_ / 4;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    limit = 0;
  }

  size_t bytes_freed = Trim(limit);
  if (bytes_freed > 0) {
    UMA_HISTOGRAM_COUNTS_100000(
        "GPU.ProgramCache.MemoryReleasedOnPressure",
        static_cast<base::HistogramBase::Sample>(bytes_freed) / 1024);
  }
}

}  // namespace gles2
}  // namespace gpu
