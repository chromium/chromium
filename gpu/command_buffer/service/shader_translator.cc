// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include <stddef.h>
#include <string.h>
#include <algorithm>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {

class ShaderTranslatorInitializer {
 public:
  ShaderTranslatorInitializer() {
    TRACE_EVENT0("gpu", "ShInitialize");
    CHECK(sh::Initialize());
  }

  ~ShaderTranslatorInitializer() {
    TRACE_EVENT0("gpu", "ShFinalize");
    sh::Finalize();
  }
};

base::LazyInstance<ShaderTranslatorInitializer>::DestructorAtExit
    g_translator_initializer = LAZY_INSTANCE_INITIALIZER;

void GetAttributes(ShHandle compiler, AttributeMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::Attribute>* attribs = sh::GetAttributes(compiler);
  if (attribs) {
    for (size_t ii = 0; ii < attribs->size(); ++ii)
      (*var_map)[(*attribs)[ii].mappedName] = (*attribs)[ii];
  }
}

void GetUniforms(ShHandle compiler, UniformMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::Uniform>* uniforms = sh::GetUniforms(compiler);
  if (uniforms) {
    for (size_t ii = 0; ii < uniforms->size(); ++ii)
      (*var_map)[(*uniforms)[ii].mappedName] = (*uniforms)[ii];
  }
}

void GetVaryings(ShHandle compiler, VaryingMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::Varying>* varyings = sh::GetVaryings(compiler);
  if (varyings) {
    for (size_t ii = 0; ii < varyings->size(); ++ii)
      (*var_map)[(*varyings)[ii].mappedName] = (*varyings)[ii];
  }
}
void GetOutputVariables(ShHandle compiler, OutputVariableList* var_list) {
  if (!var_list)
    return;
  *var_list = *sh::GetOutputVariables(compiler);
}

void GetInterfaceBlocks(ShHandle compiler, InterfaceBlockMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::InterfaceBlock>* interface_blocks =
      sh::GetInterfaceBlocks(compiler);
  if (interface_blocks) {
    for (const auto& block : *interface_blocks) {
      (*var_map)[block.mappedName] = block;
    }
  }
}

}  // namespace

ShShaderOutput ShaderTranslator::GetShaderOutputLanguageForContext(
    const gl::GLVersionInfo& version_info) {
  if (version_info.is_es) {
    return SH_ESSL_OUTPUT;
  }

  // Determine the GLSL version based on OpenGL specification.

  unsigned context_version =
      version_info.major_version * 100 + version_info.minor_version * 10;
  if (context_version >= 450) {
    // OpenGL specs from 4.2 on specify that the core profile is "also
    // guaranteed to support all previous versions of the OpenGL Shading
    // Language back to version 1.40". For simplicity, we assume future
    // specs do not unspecify this. If they did, they could unspecify
    // glGetStringi(GL_SHADING_LANGUAGE_VERSION, k), too.
    // Since current context >= 4.5, use GLSL 4.50 core.
    return SH_GLSL_450_CORE_OUTPUT;
  } else if (context_version == 440) {
    return SH_GLSL_440_CORE_OUTPUT;
  } else if (context_version == 430) {
    return SH_GLSL_430_CORE_OUTPUT;
  } else if (context_version == 420) {
    return SH_GLSL_420_CORE_OUTPUT;
  } else if (context_version == 410) {
    return SH_GLSL_410_CORE_OUTPUT;
  } else if (context_version == 400) {
    return SH_GLSL_400_CORE_OUTPUT;
  } else if (context_version == 330) {
    return SH_GLSL_330_CORE_OUTPUT;
  } else if (context_version == 320) {
    return SH_GLSL_150_CORE_OUTPUT;
  }

  // Before OpenGL 3.2 we use the compatibility profile. Shading
  // language version 130 restricted how sampler arrays can be indexed
  // in loops, which causes problems like crbug.com/550487 .
  //
  // Also for any future specs that might be introduced between OpenGL
  // 3.3 and OpenGL 4.0, at the time of writing, we use the
  // compatibility profile.
  return SH_GLSL_COMPATIBILITY_OUTPUT;
}

ShaderTranslator::DestructionObserver::DestructionObserver() = default;

ShaderTranslator::DestructionObserver::~DestructionObserver() = default;

ShaderTranslator::ShaderTranslator()
    : compiler_(nullptr), compile_options_(0) {}

bool ShaderTranslator::Init(GLenum shader_type,
                            ShShaderSpec shader_spec,
                            const ShBuiltInResources* resources,
                            ShShaderOutput shader_output_language,
                            ShCompileOptions driver_bug_workarounds,
                            bool gl_shader_interm_output) {
  // Make sure Init is called only once.
  DCHECK(compiler_ == nullptr);
  DCHECK(shader_type == GL_FRAGMENT_SHADER || shader_type == GL_VERTEX_SHADER);
  DCHECK(shader_spec == SH_GLES2_SPEC || shader_spec == SH_WEBGL_SPEC ||
         shader_spec == SH_GLES3_SPEC || shader_spec == SH_WEBGL2_SPEC);
  DCHECK(resources != nullptr);

  g_translator_initializer.Get();


  {
    TRACE_EVENT0("gpu", "ShConstructCompiler");
    compiler_ = sh::ConstructCompiler(shader_type, shader_spec,
                                      shader_output_language, resources);
  }

  compile_options_ =
      SH_OBJECT_CODE | SH_VARIABLES | SH_ENFORCE_PACKING_RESTRICTIONS |
      SH_LIMIT_EXPRESSION_COMPLEXITY | SH_LIMIT_CALL_STACK_DEPTH |
      SH_CLAMP_INDIRECT_ARRAY_BOUNDS | SH_EMULATE_GL_DRAW_ID |
      SH_EMULATE_GL_BASE_VERTEX_BASE_INSTANCE;
  if (gl_shader_interm_output)
    compile_options_ |= SH_INTERMEDIATE_TREE;
  compile_options_ |= driver_bug_workarounds;
  switch (shader_spec) {
    case SH_WEBGL_SPEC:
    case SH_WEBGL2_SPEC:
      compile_options_ |= SH_INIT_OUTPUT_VARIABLES;
      break;
    default:
      break;
  }

  if (compiler_) {
    options_affecting_compilation_ =
        base::MakeRefCounted<OptionsAffectingCompilationString>(
            std::string(":CompileOptions:" +
                        base::NumberToString(GetCompileOptions())) +
            sh::GetBuiltInResourcesString(compiler_));
  }

  return compiler_ != nullptr;
}

ShCompileOptions ShaderTranslator::GetCompileOptions() const {
  return compile_options_;
}

bool ShaderTranslator::Translate(
    const std::string& shader_source,
    std::string* info_log,
    std::string* translated_source,
    int* shader_version,
    AttributeMap* attrib_map,
    UniformMap* uniform_map,
    VaryingMap* varying_map,
    InterfaceBlockMap* interface_block_map,
    OutputVariableList* output_variable_list) const {
  // Make sure this instance is initialized.
  DCHECK(compiler_ != nullptr);

  bool success = false;
  {
    TRACE_EVENT0("gpu", "ShCompile");
    const char* const shader_strings[] = { shader_source.c_str() };
    success = sh::Compile(compiler_, shader_strings, 1, GetCompileOptions());
  }
  if (success) {
    // Get translated shader.
    if (translated_source) {
      *translated_source = sh::GetObjectCode(compiler_);
    }
    // Get shader version.
    *shader_version = sh::GetShaderVersion(compiler_);
    // Get info for attribs, uniforms, varyings and output variables.
    GetAttributes(compiler_, attrib_map);
    GetUniforms(compiler_, uniform_map);
    GetVaryings(compiler_, varying_map);
    GetInterfaceBlocks(compiler_, interface_block_map);
    GetOutputVariables(compiler_, output_variable_list);
  }

  // Get info log.
  if (info_log) {
    *info_log = sh::GetInfoLog(compiler_);
  }

  // We don't need results in the compiler anymore.
  sh::ClearResults(compiler_);

  return success;
}

OptionsAffectingCompilationString*
ShaderTranslator::GetStringForOptionsThatWouldAffectCompilation() const {
  return options_affecting_compilation_.get();
}

void ShaderTranslator::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void ShaderTranslator::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

ShaderTranslator::~ShaderTranslator() {
  for (auto& observer : destruction_observers_)
    observer.OnDestruct(this);

  if (compiler_ != nullptr)
    sh::Destruct(compiler_);
}

}  // namespace gles2
}  // namespace gpu

