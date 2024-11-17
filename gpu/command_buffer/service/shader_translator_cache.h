// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_CACHE_H_

#include <string.h>

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/angle/include/GLSLANG/ShaderLang.h"

namespace gpu {

struct GpuPreferences;

namespace gles2 {

// This class is not thread safe and can only be created and destroyed
// on a single thread. But it is safe to use two independent instances on two
// threads without synchronization.
//
// TODO(backer): Investigate using glReleaseShaderCompiler as an alternative to
// to this cache.
class GPU_GLES2_EXPORT ShaderTranslatorCache
    : public ShaderTranslator::DestructionObserver {
 public:
  explicit ShaderTranslatorCache(const GpuPreferences& gpu_preferences);

  ShaderTranslatorCache(const ShaderTranslatorCache&) = delete;
  ShaderTranslatorCache& operator=(const ShaderTranslatorCache&) = delete;

  ~ShaderTranslatorCache() override;

  // ShaderTranslator::DestructionObserver implementation
  void OnDestruct(ShaderTranslator* translator) override;

  scoped_refptr<ShaderTranslator> GetTranslator(
      sh::GLenum shader_type,
      ShShaderSpec shader_spec,
      const ShBuiltInResources* resources,
      ShShaderOutput shader_output_language,
      const ShCompileOptions& driver_bug_workarounds);

 private:
  friend class ShaderTranslatorCacheTest_InitParamComparable_Test;

  // Parameters passed into ShaderTranslator::Init
  struct ShaderTranslatorInitParams {
    sh::GLenum shader_type;
    ShShaderSpec shader_spec;
    ShBuiltInResources resources;
    ShShaderOutput shader_output_language;
    ShCompileOptions driver_bug_workarounds;

    ShaderTranslatorInitParams(sh::GLenum shader_type,
                               ShShaderSpec shader_spec,
                               const ShBuiltInResources& resources,
                               ShShaderOutput shader_output_language,
                               const ShCompileOptions& driver_bug_workarounds) {
      memset(this, 0, sizeof(*this));
      this->shader_type = shader_type;
      this->shader_spec = shader_spec;
      this->resources = resources;
      this->shader_output_language = shader_output_language;
      this->driver_bug_workarounds = driver_bug_workarounds;
    }

    ShaderTranslatorInitParams(const ShaderTranslatorInitParams& params) {
      memcpy(this, &params, sizeof(*this));
    }

    bool operator== (const ShaderTranslatorInitParams& params) const {
      return memcmp(&params, this, sizeof(*this)) == 0;
    }

    bool operator< (const ShaderTranslatorInitParams& params) const {
      return memcmp(&params, this, sizeof(*this)) < 0;
    }

   private:
    ShaderTranslatorInitParams() = delete;
    ShaderTranslatorInitParams& operator=(const ShaderTranslatorInitParams&) =
        delete;
  };

  const GpuPreferences gpu_preferences_;

  typedef std::map<ShaderTranslatorInitParams,
                   raw_ptr<ShaderTranslator, CtnExperimental>>
      Cache;
  Cache cache_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_CACHE_H_
