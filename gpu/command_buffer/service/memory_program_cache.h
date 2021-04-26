// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_PROGRAM_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_PROGRAM_CACHE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/shader_translator.h"

namespace gpu {

class GpuProcessActivityFlags;

namespace gles2 {

// Program cache that stores binaries completely in-memory
class GPU_GLES2_EXPORT MemoryProgramCache : public ProgramCache {
 public:
  MemoryProgramCache(size_t max_cache_size_bytes,
                     bool disable_gpu_shader_disk_cache,
                     bool disable_program_caching_for_transform_feedback,
                     GpuProcessActivityFlags* activity_flags);
  ~MemoryProgramCache() override;

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

 private:
  void ClearBackend() override;

  class ProgramCacheValue : public base::RefCounted<ProgramCacheValue> {
   public:
    ProgramCacheValue(GLenum format,
                      std::vector<uint8_t> data,
                      bool is_compressed,
                      GLsizei decompressed_length,
                      const std::string& program_hash,
                      const char* shader_0_hash,
                      const AttributeMap& attrib_map_0,
                      const UniformMap& uniform_map_0,
                      const VaryingMap& varying_map_0,
                      const OutputVariableList& output_variable_list_0,
                      const InterfaceBlockMap& interface_block_map_0,
                      const char* shader_1_hash,
                      const AttributeMap& attrib_map_1,
                      const UniformMap& uniform_map_1,
                      const VaryingMap& varying_map_1,
                      const OutputVariableList& output_variable_list_1,
                      const InterfaceBlockMap& interface_block_map_1,
                      MemoryProgramCache* program_cache);

    GLenum format() const {
      return format_;
    }

    const std::vector<uint8_t>& data() const { return data_; }

    bool is_compressed() const { return is_compressed_; }

    GLsizei decompressed_length() const { return decompressed_length_; }

    const std::string& shader_0_hash() const {
      return shader_0_hash_;
    }

    const AttributeMap& attrib_map_0() const {
      return attrib_map_0_;
    }

    const UniformMap& uniform_map_0() const {
      return uniform_map_0_;
    }

    const VaryingMap& varying_map_0() const {
      return varying_map_0_;
    }

    const OutputVariableList& output_variable_list_0() const {
      return output_variable_list_0_;
    }

    const InterfaceBlockMap& interface_block_map_0() const {
      return interface_block_map_0_;
    }

    const std::string& shader_1_hash() const {
      return shader_1_hash_;
    }

    const AttributeMap& attrib_map_1() const {
      return attrib_map_1_;
    }

    const UniformMap& uniform_map_1() const {
      return uniform_map_1_;
    }

    const VaryingMap& varying_map_1() const {
      return varying_map_1_;
    }

    const OutputVariableList& output_variable_list_1() const {
      return output_variable_list_1_;
    }

    const InterfaceBlockMap& interface_block_map_1() const {
      return interface_block_map_1_;
    }

   private:
    friend class base::RefCounted<ProgramCacheValue>;

    ~ProgramCacheValue();

    const GLenum format_;
    const std::vector<uint8_t> data_;
    const bool is_compressed_;
    const GLsizei decompressed_length_;
    const std::string program_hash_;
    const std::string shader_0_hash_;
    const AttributeMap attrib_map_0_;
    const UniformMap uniform_map_0_;
    const VaryingMap varying_map_0_;
    const OutputVariableList output_variable_list_0_;
    const InterfaceBlockMap interface_block_map_0_;
    const std::string shader_1_hash_;
    const AttributeMap attrib_map_1_;
    const UniformMap uniform_map_1_;
    const VaryingMap varying_map_1_;
    const OutputVariableList output_variable_list_1_;
    const InterfaceBlockMap interface_block_map_1_;
    MemoryProgramCache* const program_cache_;

    DISALLOW_COPY_AND_ASSIGN(ProgramCacheValue);
  };

  friend class ProgramCacheValue;

  typedef base::MRUCache<std::string,
                         scoped_refptr<ProgramCacheValue> > ProgramMRUCache;

  const bool disable_gpu_shader_disk_cache_;
  const bool disable_program_caching_for_transform_feedback_;
  const bool compress_program_binaries_;
  size_t curr_size_bytes_;
  ProgramMRUCache store_;
  GpuProcessActivityFlags* activity_flags_;

  DISALLOW_COPY_AND_ASSIGN(MemoryProgramCache);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_PROGRAM_CACHE_H_
