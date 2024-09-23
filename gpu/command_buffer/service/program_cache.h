// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/stack_allocated.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class DecoderClient;

namespace gles2 {

class Shader;

// Program cache base class for caching linked gpu programs
class GPU_GLES2_EXPORT ProgramCache {
 public:
  static const size_t kHashLength = base::kSHA1Length;
  using Hash = std::array<uint8_t, kHashLength>;
  using HashView = base::span<const uint8_t, kHashLength>;

  typedef std::map<std::string, GLint> LocationMap;
  using CacheProgramCallback =
      ::base::RepeatingCallback<void(const std::string&, const std::string&)>;

  enum LinkedProgramStatus {
    LINK_UNKNOWN,
    LINK_SUCCEEDED
  };

  enum ProgramLoadResult {
    PROGRAM_LOAD_FAILURE,
    PROGRAM_LOAD_SUCCESS
  };

  class GPU_GLES2_EXPORT ScopedCacheUse {
    STACK_ALLOCATED();

   public:
    ScopedCacheUse(ProgramCache* cache, CacheProgramCallback callback);
    // Disallow copy/assign as it is subtle and error prone (only one
    // ScopedCacheUse should reset the callback on destruction).
    ScopedCacheUse(const ScopedCacheUse& other) = delete;
    ScopedCacheUse& operator=(const ScopedCacheUse& other) = delete;
    // Disallow move as the destructor dereferences `cache_` after it has been
    // moved out.
    ScopedCacheUse(ScopedCacheUse&& other) = delete;
    ScopedCacheUse& operator=(ScopedCacheUse&& other) = delete;
    ~ScopedCacheUse();

   private:
    ProgramCache* cache_;
  };

  explicit ProgramCache(size_t max_cache_size_bytes);

  ProgramCache(const ProgramCache&) = delete;
  ProgramCache& operator=(const ProgramCache&) = delete;

  virtual ~ProgramCache();

  bool HasSuccessfullyCompiledShader(const std::string& shader_signature) const;

  LinkedProgramStatus GetLinkedProgramStatus(
      const std::string& shader_signature_a,
      const std::string& shader_signature_b,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode) const;

  // Loads the linked program from the cache.  If the program is not found or
  // there was an error, PROGRAM_LOAD_FAILURE should be returned.
  virtual ProgramLoadResult LoadLinkedProgram(
      GLuint program,
      Shader* shader_a,
      Shader* shader_b,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      DecoderClient* client) = 0;

  // Saves the program into the cache.  If successful, the implementation should
  // call LinkedProgramCacheSuccess.
  virtual void SaveLinkedProgram(
      GLuint program,
      const Shader* shader_a,
      const Shader* shader_b,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      DecoderClient* client) = 0;

  virtual void LoadProgram(const std::string& key,
                           const std::string& program) = 0;

  // clears the cache
  void Clear();

  // Only for testing
  void LinkedProgramCacheSuccess(const std::string& shader_signature_a,
       const std::string& shader_signature_b,
       const LocationMap* bind_attrib_location_map,
       const std::vector<std::string>& transform_feedback_varyings,
       GLenum transform_feedback_buffer_mode);

  // Discards excess cache contents to a fixed upper limit.
  // Returns the number of bytes of memory freed.
  virtual size_t Trim(size_t limit) = 0;

  // Reduces cache usage based on the given MemoryPressureLevel
  void HandleMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

 protected:
  size_t max_size_bytes() const { return max_size_bytes_; }

  // called by implementing class after a shader was successfully cached
  void LinkedProgramCacheSuccess(const Hash& program_hash);

  void CompiledShaderCacheSuccess(const Hash& shader_hash);

  void ComputeShaderHash(std::string_view shader, Hash& result) const;

  void ComputeProgramHash(
      HashView hashed_shader_0,
      HashView hashed_shader_1,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      Hash& result) const;

  // TODO(dcheng): Maybe this can take a HashView.
  void Evict(const Hash& program_hash,
             const Hash& shader_0_hash,
             const Hash& shader_1_hash);

  // Will also be used by derived class to guard some members.
  mutable base::Lock lock_;

  // Used by the passthrough program cache to notify when a new blob is
  // inserted.
  CacheProgramCallback cache_program_callback_ GUARDED_BY(lock_);

 private:
  struct HashHasher {
    size_t operator()(const Hash& hash) const;
  };

  using LinkStatusMap =
      std::unordered_map<Hash, LinkedProgramStatus, HashHasher>;
  using CachedCompiledShaderSet = std::unordered_set<Hash, HashHasher>;

  // called to clear the backend cache
  virtual void ClearBackend() = 0;

  const size_t max_size_bytes_;
  LinkStatusMap link_status_;
  // only cache the hash of successfully compiled shaders
  CachedCompiledShaderSet compiled_shaders_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_H_
