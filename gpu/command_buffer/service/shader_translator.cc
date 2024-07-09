// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include <stddef.h>
#include <string.h>
#include <algorithm>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

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

ShaderTranslator::DestructionObserver::DestructionObserver() = default;

ShaderTranslator::DestructionObserver::~DestructionObserver() = default;

ShaderTranslator::ShaderTranslator() : compiler_(nullptr), compile_options_{} {}

bool ShaderTranslator::Init(GLenum shader_type,
                            ShShaderSpec shader_spec,
                            const ShBuiltInResources* resources,
                            ShShaderOutput shader_output_language,
                            const ShCompileOptions& driver_bug_workarounds,
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

  compile_options_ = driver_bug_workarounds;
  compile_options_.objectCode = true;
  compile_options_.enforcePackingRestrictions = true;
  compile_options_.limitExpressionComplexity = true;
  compile_options_.limitCallStackDepth = true;
  compile_options_.clampIndirectArrayBounds = true;
  compile_options_.emulateGLDrawID = true;
  compile_options_.emulateGLBaseVertexBaseInstance = true;

  std::string compile_options_string =
      "objectCode:variables:enforcePackingRestrictions:"
      "limitExpressionComplexity:limitCallStackDepth:clampIndirectArrayBounds:"
      "emulateGLDrawID:emulateGLBaseVertexBaseInstance";

  if (gl_shader_interm_output) {
    compile_options_.intermediateTree = true;
    compile_options_string += ":intermediateTree";
  }

  switch (shader_spec) {
    case SH_WEBGL_SPEC:
    case SH_WEBGL2_SPEC:
      compile_options_.initOutputVariables = true;
      break;
    default:
      break;
  }

  // Build the options string for additional features that may be set by the
  // caller.  Note that this code is used by the validating command decoder,
  // which is deprecated.  No new features are expected to be enabled, neither
  // is it expected for there to be new users of this code.
  if (compile_options_.initOutputVariables)
    compile_options_string += ":initOutputVariables";
  if (compile_options_.initGLPosition)
    compile_options_string += ":initGLPosition";
  if (compile_options_.unfoldShortCircuit)
    compile_options_string += ":unfoldShortCircuit";
  if (compile_options_.scalarizeVecAndMatConstructorArgs)
    compile_options_string += ":scalarizeVecAndMatConstructorArgs";
  if (compile_options_.regenerateStructNames)
    compile_options_string += ":regenerateStructNames";
  if (compile_options_.emulateAbsIntFunction)
    compile_options_string += ":emulateAbsIntFunction";
  if (compile_options_.rewriteTexelFetchOffsetToTexelFetch)
    compile_options_string += ":rewriteTexelFetchOffsetToTexelFetch";
  if (compile_options_.addAndTrueToLoopCondition)
    compile_options_string += ":addAndTrueToLoopCondition";
  if (compile_options_.rewriteDoWhileLoops)
    compile_options_string += ":rewriteDoWhileLoops";
  if (compile_options_.emulateIsnanFloatFunction)
    compile_options_string += ":emulateIsnanFloatFunction";
  if (compile_options_.useUnusedStandardSharedBlocks)
    compile_options_string += ":useUnusedStandardSharedBlocks";
  if (compile_options_.removeInvariantAndCentroidForESSL3)
    compile_options_string += ":removeInvariantAndCentroidForESSL3";
  if (compile_options_.rewriteFloatUnaryMinusOperator)
    compile_options_string += ":rewriteFloatUnaryMinusOperator";
  if (compile_options_.dontUseLoopsToInitializeVariables)
    compile_options_string += ":dontUseLoopsToInitializeVariables";
  if (compile_options_.removeDynamicIndexingOfSwizzledVector)
    compile_options_string += ":removeDynamicIndexingOfSwizzledVector";
  if (compile_options_.initializeUninitializedLocals)
    compile_options_string += ":initializeUninitializedLocals";

  if (compiler_) {
    options_affecting_compilation_ =
        base::MakeRefCounted<OptionsAffectingCompilationString>(
            ":CompileOptions:" + compile_options_string +
            sh::GetBuiltInResourcesString(compiler_));
  }

  return compiler_ != nullptr;
}

const ShCompileOptions& ShaderTranslator::GetCompileOptions() const {
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
