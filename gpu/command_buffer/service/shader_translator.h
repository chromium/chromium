// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_

#include <string>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/angle/include/GLSLANG/ShaderLang.h"

namespace gpu {
namespace gles2 {

// Mapping between variable name and info.
typedef std::unordered_map<std::string, sh::Attribute> AttributeMap;
typedef std::vector<sh::OutputVariable> OutputVariableList;
typedef std::unordered_map<std::string, sh::Uniform> UniformMap;
typedef std::unordered_map<std::string, sh::Varying> VaryingMap;
typedef std::unordered_map<std::string, sh::InterfaceBlock> InterfaceBlockMap;
typedef base::RefCountedData<std::string> OptionsAffectingCompilationString;

// Translates a GLSL ES 2.0 shader to desktop GLSL shader, or just
// validates GLSL ES 2.0 shaders on a true GLSL ES implementation.
class ShaderTranslatorInterface
    : public base::RefCounted<ShaderTranslatorInterface> {
 public:
  ShaderTranslatorInterface() = default;

  ShaderTranslatorInterface(const ShaderTranslatorInterface&) = delete;
  ShaderTranslatorInterface& operator=(const ShaderTranslatorInterface&) =
      delete;

  // Initializes the translator.
  // Must be called once before using the translator object.
  virtual bool Init(sh::GLenum shader_type,
                    ShShaderSpec shader_spec,
                    const ShBuiltInResources* resources,
                    ShShaderOutput shader_output_language,
                    const ShCompileOptions& driver_bug_workarounds,
                    bool gl_shader_interm_output) = 0;

  // Translates the given shader source.
  // Returns true if translation is successful, false otherwise.
  // Always fill |info_log| if it's non-null.
  // Upon success, fill |translated_shader|, |attrib_map|, |uniform_map|,
  // |varying_map|, and |name_map| if they are non-null.
  virtual bool Translate(const std::string& shader_source,
                         std::string* info_log,
                         std::string* translated_shader,
                         int* shader_version,
                         AttributeMap* attrib_map,
                         UniformMap* uniform_map,
                         VaryingMap* varying_map,
                         InterfaceBlockMap* interface_block_map,
                         OutputVariableList* output_variable_list) const = 0;

  // Return a string that is unique for a specfic set of options that would
  // possibly affect compilation.
  virtual OptionsAffectingCompilationString*
  GetStringForOptionsThatWouldAffectCompilation() const = 0;

 protected:
  virtual ~ShaderTranslatorInterface() = default;

 private:
  friend class base::RefCounted<ShaderTranslatorInterface>;
};

// Implementation of ShaderTranslatorInterface
class GPU_GLES2_EXPORT ShaderTranslator : public ShaderTranslatorInterface {
 public:
  class DestructionObserver {
   public:
    DestructionObserver();

    DestructionObserver(const DestructionObserver&) = delete;
    DestructionObserver& operator=(const DestructionObserver&) = delete;

    virtual ~DestructionObserver();

    virtual void OnDestruct(ShaderTranslator* translator) = 0;
  };

  ShaderTranslator();

  // Overridden from ShaderTranslatorInterface.
  bool Init(sh::GLenum shader_type,
            ShShaderSpec shader_spec,
            const ShBuiltInResources* resources,
            ShShaderOutput shader_output_language,
            const ShCompileOptions& driver_bug_workarounds,
            bool gl_shader_interm_output) override;

  // Overridden from ShaderTranslatorInterface.
  bool Translate(const std::string& shader_source,
                 std::string* info_log,
                 std::string* translated_source,
                 int* shader_version,
                 AttributeMap* attrib_map,
                 UniformMap* uniform_map,
                 VaryingMap* varying_map,
                 InterfaceBlockMap* interface_block_map,
                 OutputVariableList* output_variable_list) const override;

  OptionsAffectingCompilationString*
  GetStringForOptionsThatWouldAffectCompilation() const override;

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

 private:
  ~ShaderTranslator() override;

  const ShCompileOptions& GetCompileOptions() const;

  ShHandle compiler_;
  ShCompileOptions compile_options_;
  scoped_refptr<OptionsAffectingCompilationString>
      options_affecting_compilation_;
  base::ObserverList<DestructionObserver>::Unchecked destruction_observers_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_TRANSLATOR_H_
