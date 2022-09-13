// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

#include <string>
#include <unordered_map>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class ProgressReporter;
}

namespace gpu {
namespace gles2 {

enum ShaderVariableBaseType {
  SHADER_VARIABLE_INT = 0x01,
  SHADER_VARIABLE_UINT = 0x02,
  SHADER_VARIABLE_FLOAT = 0x03,
  SHADER_VARIABLE_UNDEFINED_TYPE = 0x00
};

// Compiles shader_source into shader and gives informative logging if
// the compilation fails.
void CompileShaderWithLog(GLuint shader, const char* shader_source);

// This is used to keep the source code for a shader. This is because in order
// to emluate GLES2 the shaders will have to be re-written before passed to
// the underlying OpenGL. But, when the user calls glGetShaderSource they
// should get the source they passed in, not the re-written source.
class GPU_GLES2_EXPORT Shader : public base::RefCounted<Shader> {
 public:
  enum TranslatedShaderSourceType {
    kANGLE,
    kGL,  // GL or GLES
  };

  enum ShaderState {
    kShaderStateWaiting,
    kShaderStateCompileRequested,
    kShaderStateCompiled, // Signifies compile happened, not valid compile.
  };

  static const int kUndefinedShaderVersion = -1;

  void RequestCompile(scoped_refptr<ShaderTranslatorInterface> translator,
                      TranslatedShaderSourceType type);

  // Returns true if we are ready to call DoCompile. If we have not yet called
  // RequestCompile or if we've already compiled, returns false.
  bool CanCompile() { return shader_state_ == kShaderStateCompileRequested; }
  bool HasCompiled() { return shader_state_ == kShaderStateCompiled; }
  void DoCompile();
  void RefreshTranslatedShaderSource();

  ShaderState shader_state() const {
    return shader_state_;
  }

  GLuint service_id() const {
    return service_id_;
  }

  GLenum shader_type() const {
    return shader_type_;
  }

  int shader_version() const {
    return shader_version_;
  }

  const std::string& source() const {
    return source_;
  }

  void set_source(const std::string& source) {
    source_ = source;
  }

  const std::string& translated_source() const {
    return translated_source_;
  }

  std::string last_compiled_source() const {
    return last_compiled_source_;
  }

  std::string last_compiled_signature() const {
    if (options_affecting_compilation_) {
      return last_compiled_source_ + options_affecting_compilation_->data;
    }
    return last_compiled_source_;
  }

  const sh::Attribute* GetAttribInfo(const std::string& name) const;
  const sh::Uniform* GetUniformInfo(const std::string& name) const;
  const sh::Varying* GetVaryingInfo(const std::string& name) const;
  const sh::InterfaceBlock* GetInterfaceBlockInfo(
      const std::string& name) const;
  const sh::OutputVariable* GetOutputVariableInfo(
      const std::string& name) const;

  // If the original_name is not found, return nullptr.
  const std::string* GetAttribMappedName(
      const std::string& original_name) const;

  // If the original_name is not found, return nullptr.
  const std::string* GetUniformMappedName(
      const std::string& original_name) const;

  // If the original_name is not found, return nullptr.
  const std::string* GetVaryingMappedName(
      const std::string& original_name) const;

  // If the original_name is not found, return nullptr.
  const std::string* GetInterfaceBlockMappedName(
      const std::string& original_name) const;

  // If the original_name is not found, return nullptr.
  const std::string* GetOutputVariableMappedName(
      const std::string& original_name) const;

  // If the hashed_name is not found, return nullptr.
  // Use this only when one of the more specific Get*Info methods can't be used.
  const std::string* GetOriginalNameFromHashedName(
      const std::string& hashed_name) const;

  const std::string& log_info() const {
    return log_info_;
  }

  bool valid() const {
    return shader_state_ == kShaderStateCompiled && valid_;
  }

  bool IsDeleted() const {
    return marked_for_deletion_;
  }

  bool InUse() const {
    DCHECK_GE(use_count_, 0);
    return use_count_ != 0;
  }

  // Used by program cache.
  const AttributeMap& attrib_map() const {
    return attrib_map_;
  }

  // Used by program cache.
  const UniformMap& uniform_map() const {
    return uniform_map_;
  }

  // Used by program cache.
  const VaryingMap& varying_map() const {
    return varying_map_;
  }

  const OutputVariableList& output_variable_list() const {
    return output_variable_list_;
  }

  // Used by program cache.
  const InterfaceBlockMap& interface_block_map() const {
    return interface_block_map_;
  }

  // Used by program cache.
  void set_attrib_map(const AttributeMap& attrib_map) {
    // copied because cache might be cleared
    attrib_map_ = AttributeMap(attrib_map);
  }

  // Used by program cache.
  void set_uniform_map(const UniformMap& uniform_map) {
    // copied because cache might be cleared
    uniform_map_ = UniformMap(uniform_map);
  }

  // Used by program cache.
  void set_varying_map(const VaryingMap& varying_map) {
    // copied because cache might be cleared
    varying_map_ = VaryingMap(varying_map);
  }

  // Used by program cache.
  void set_interface_block_map(const InterfaceBlockMap& interface_block_map) {
    // copied because cache might be cleared
    interface_block_map_ = InterfaceBlockMap(interface_block_map);
  }

  void set_output_variable_list(
      const OutputVariableList& output_variable_list) {
    // copied because cache might be cleared
    output_variable_list_ = output_variable_list;
  }

 private:
  friend class base::RefCounted<Shader>;
  friend class ShaderManager;

  Shader(GLuint service_id, GLenum shader_type);
  ~Shader();

  // Must be called only if we currently own the context. Forces the deletion
  // of the underlying shader service id.
  void Destroy();

  void IncUseCount() { ++use_count_; }

  void DecUseCount() {
    --use_count_;
    DCHECK_GE(use_count_, 0);
  }

  void MarkForDeletion();
  void DeleteServiceID();

  int use_count_;

  // The current state of the shader.
  ShaderState shader_state_;

  // The shader has been marked for deletion.
  bool marked_for_deletion_;

  // The shader this Shader is tracking.
  GLuint service_id_;

  // Type of shader - GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
  GLenum shader_type_;

  // Version of the shader. Can be kUndefinedShaderVersion or version returned
  // by ANGLE.
  int shader_version_;

  // Translated source type when shader was last requested to be compiled.
  TranslatedShaderSourceType source_type_;

  // Translator to use, set when shader was last requested to be compiled.
  scoped_refptr<ShaderTranslatorInterface> translator_;
  scoped_refptr<OptionsAffectingCompilationString>
      options_affecting_compilation_;

  // True if compilation succeeded.
  bool valid_;

  // The shader source as passed to glShaderSource.
  std::string source_;

  // The source the last compile used.
  std::string last_compiled_source_;

  // The translated shader source.
  std::string translated_source_;

  // The shader translation log.
  std::string log_info_;

  // The type info when the shader was last compiled.
  AttributeMap attrib_map_;
  UniformMap uniform_map_;
  VaryingMap varying_map_;
  InterfaceBlockMap interface_block_map_;
  OutputVariableList output_variable_list_;
  // If a new info type is added, add it to GetOriginalNameFromHashedName.
};

// Tracks the Shaders.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class GPU_GLES2_EXPORT ShaderManager {
 public:
  ShaderManager(gl::ProgressReporter* progress_reporter);

  ShaderManager(const ShaderManager&) = delete;
  ShaderManager& operator=(const ShaderManager&) = delete;

  ~ShaderManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a shader for the given shader ID.
  Shader* CreateShader(
      GLuint client_id,
      GLuint service_id,
      GLenum shader_type);

  // Gets an existing shader info for the given shader ID. Returns nullptr if
  // none exists.
  Shader* GetShader(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  void Delete(Shader* shader);

  // Mark a shader as used
  void UseShader(Shader* shader);

  // Unmark a shader as used. If it has been deleted and is not used
  // then we free the shader.
  void UnuseShader(Shader* shader);

  // Check if a Shader is owned by this ShaderManager.
  bool IsOwned(Shader* shader);

 private:
  friend class Shader;

  // Info for each shader by service side shader Id.
  typedef std::unordered_map<GLuint, scoped_refptr<Shader>> ShaderMap;
  ShaderMap shaders_;

  void RemoveShaderIfUnused(Shader* shader);

  // Used to notify the watchdog thread of progress during destruction,
  // preventing time-outs when destruction takes a long time. May be null when
  // using in-process command buffer.
  raw_ptr<gl::ProgressReporter> progress_reporter_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_
