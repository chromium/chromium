// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator_cache.h"

namespace gpu {
namespace gles2 {

ShaderTranslatorCache::ShaderTranslatorCache(
    const GpuPreferences& gpu_preferences)
    : gpu_preferences_(gpu_preferences) {
}

ShaderTranslatorCache::~ShaderTranslatorCache() {
  DCHECK(cache_.empty());
}

void ShaderTranslatorCache::OnDestruct(ShaderTranslator* translator) {
  Cache::iterator it = cache_.begin();
  while (it != cache_.end()) {
    if (it->second == translator) {
      cache_.erase(it);
      return;
    }
    it++;
  }
}

scoped_refptr<ShaderTranslator> ShaderTranslatorCache::GetTranslator(
    sh::GLenum shader_type,
    ShShaderSpec shader_spec,
    const ShBuiltInResources* resources,
    ShShaderOutput shader_output_language,
    const ShCompileOptions& driver_bug_workarounds) {
  ShaderTranslatorInitParams params(shader_type, shader_spec, *resources,
                                    shader_output_language,
                                    driver_bug_workarounds);

  Cache::iterator it = cache_.find(params);
  if (it != cache_.end())
    return it->second.get();

  ShaderTranslator* translator = new ShaderTranslator();
  if (translator->Init(shader_type, shader_spec, resources,
                       shader_output_language, driver_bug_workarounds,
                       gpu_preferences_.gl_shader_interm_output)) {
    cache_[params] = translator;
    translator->AddDestructionObserver(this);
    return translator;
  } else {
    return nullptr;
  }
}

}  // namespace gles2
}  // namespace gpu
