// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_PROGRAM_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_PROGRAM_CACHE_H_

#include <mutex>
#include "base/containers/lru_cache.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

namespace gles2 {

// Program cache that stores binaries in memory, with the ability to serialize
// them for disk storage.  It also acts as generic blob cache for the underlying
// implementation via the blob cache extension.
class GPU_GLES2_EXPORT PassthroughProgramCache : public ProgramCache {
 public:
  PassthroughProgramCache(size_t max_cache_size_bytes,
                          bool disable_gpu_shader_disk_cache);

  PassthroughProgramCache(const PassthroughProgramCache&) = delete;
  PassthroughProgramCache& operator=(const PassthroughProgramCache&) = delete;

  ~PassthroughProgramCache() override;

  ProgramLoadResult LoadLinkedProgram(
      GLuint program,
      Shader* shader_a,
      Shader* shader_b,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      DecoderClient* client) override;
  void SaveLinkedProgram(
      GLuint program,
      const Shader* shader_a,
      const Shader* shader_b,
      const LocationMap* bind_attrib_location_map,
      const std::vector<std::string>& transform_feedback_varyings,
      GLenum transform_feedback_buffer_mode,
      DecoderClient* client) override;

  void LoadProgram(const std::string& key, const std::string& program) override;

  size_t Trim(size_t limit) override;

  static void BlobCacheSet(const void* key,
                           EGLsizeiANDROID key_size,
                           const void* value,
                           EGLsizeiANDROID value_size);

  static EGLsizeiANDROID BlobCacheGet(const void* key,
                                      EGLsizeiANDROID key_size,
                                      void* value,
                                      EGLsizeiANDROID value_size);

 private:
  typedef std::vector<uint8_t> Key;
  typedef std::vector<uint8_t> Value;

  class ProgramCacheValue {
   public:
    ProgramCacheValue(Value&& program_blob,
                      PassthroughProgramCache* program_cache);

    ProgramCacheValue(const ProgramCacheValue&) = delete;
    ProgramCacheValue& operator=(const ProgramCacheValue&) = delete;

    ~ProgramCacheValue();

    ProgramCacheValue(ProgramCacheValue&& other);
    ProgramCacheValue& operator=(ProgramCacheValue&& other);

    const Value& data() const { return program_blob_; }

   private:
    Value program_blob_;

    // TODO(bartekn): Change this into raw_ptr<...>, after investigating an
    // earlier crash report most likely caused by a use-after-move.
    PassthroughProgramCache* program_cache_;
  };

  void ClearBackend() override;
  bool CacheEnabled() const;

  void Set(Key&& key, Value&& value);
  const ProgramCacheValue* Get(const Key& key);

  friend class ProgramCacheValue;

  typedef base::LRUCache<Key, ProgramCacheValue> ProgramLRUCache;

  const bool disable_gpu_shader_disk_cache_;
  size_t curr_size_bytes_;
  ProgramLRUCache store_;

  // TODO(syoussefi): take compression from memory_program_cache, see
  // compress_program_binaries_
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_PROGRAM_CACHE_H_
