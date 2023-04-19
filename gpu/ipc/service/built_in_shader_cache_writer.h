// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_BUILT_IN_SHADER_CACHE_WRITER_H_
#define GPU_IPC_SERVICE_BUILT_IN_SHADER_CACHE_WRITER_H_

#include "base/files/file.h"
#include "gpu/command_buffer/service/passthrough_program_cache.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"

namespace base {
class FilePath;
}

namespace gpu {

// Used to write the shader cache to disk. Generating the shader libraries
// requires executing command line programs and is slow. Because of this,
// writing the shaders is done as part of the build process. That is, a script
// runs chrome with a set of command line arguments that results in this class
// being used.
//
// As the gpu process does not cleanly shutdown, this writes as it's going.
class GPU_IPC_SERVICE_EXPORT BuiltInShaderCacheWriter
    : public gles2::PassthroughProgramCache::ValueAddedHook {
 public:
  // Written to beginning of file to ensure file was created by this code.
  static const uint32_t kSignature;

  explicit BuiltInShaderCacheWriter(
      const base::FilePath& path = base::FilePath());
  ~BuiltInShaderCacheWriter() override;

  void OnValueAddedToCache(const std::vector<uint8_t>& key,
                           const std::vector<uint8_t>& value) override;

 private:
  bool WriteVectorToFile(const std::vector<uint8_t>& key);

  base::File file_;

  // Set to false if an an error is encountered when writing.
  bool valid_file_ = false;

  // Number of entries added.
  int add_count_ = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_BUILT_IN_SHADER_CACHE_WRITER_H_
