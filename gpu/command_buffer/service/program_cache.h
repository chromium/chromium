// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sha1.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"

namespace gpu {
namespace gles2 {

class Shader;

// Program cache base class for caching linked gpu programs
class GPU_GLES2_EXPORT ProgramCache {
 public:
  static const size_t kHashLength = base::kSHA1Length;

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

  explicit ProgramCache(size_t max_cache_size_bytes);
  virtual ~ProgramCache();

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

  void SetCacheProgramCallback(CacheProgramCallback callback);
  void ResetCacheProgramCallback();

 protected:
  size_t max_size_bytes() const { return max_size_bytes_; }

  // called by implementing class after a shader was successfully cached
  void LinkedProgramCacheSuccess(const std::string& program_hash);

  // result is not null terminated
  void ComputeShaderHash(const std::string& shader,
                         char* result) const;

  // result is not null terminated.  hashed shaders are expected to be
  // kHashLength in length
  void ComputeProgramHash(
      const char* hashed_shader_0,
      const char* hashed_shader_1,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      char* result) const;

  void Evict(const std::string& program_hash);

  // Used by the passthrough program cache to notify when a new blob is
  // inserted.
  CacheProgramCallback cache_program_callback_;

 private:
  typedef base::hash_map<std::string,
                         LinkedProgramStatus> LinkStatusMap;

  // called to clear the backend cache
  virtual void ClearBackend() = 0;

  const size_t max_size_bytes_;
  LinkStatusMap link_status_;

  DISALLOW_COPY_AND_ASSIGN(ProgramCache);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_H_
