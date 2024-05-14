// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {
namespace gles2 {

namespace {

// Given a variable name | a[0].b.c[0] |, return |a|.
std::string GetTopVariableName(const std::string& fullname) {
  size_t pos = fullname.find_first_of("[.");
  if (pos == std::string::npos)
    return fullname;
  return fullname.substr(0, pos);
}

}  // namespace anonymous

void CompileShaderWithLog(GLuint shader, const char* shader_source) {
  glShaderSource(shader, 1, &shader_source, 0);
  glCompileShader(shader);
#if DCHECK_IS_ON()
  GLint compile_status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (GL_TRUE != compile_status) {
    GLint info_log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
    std::vector<GLchar> info_log(info_log_length);
    glGetShaderInfoLog(shader, info_log_length, nullptr, &info_log[0]);
    std::string log(&info_log[0], info_log_length - 1);
    DLOG(ERROR) << "Error compiling shader: " << log;
    DLOG(ERROR) << "Shader compilation failure.";
  }
#endif
}

Shader::Shader(GLuint service_id, GLenum shader_type)
      : use_count_(0),
        shader_state_(kShaderStateWaiting),
        marked_for_deletion_(false),
        service_id_(service_id),
        shader_type_(shader_type),
        shader_version_(kUndefinedShaderVersion),
        source_type_(kANGLE),
        valid_(false) {
}

Shader::~Shader() = default;

void Shader::Destroy() {
  DeleteServiceID();
}

void Shader::RequestCompile(scoped_refptr<ShaderTranslatorInterface> translator,
                            TranslatedShaderSourceType type) {
  shader_state_ = kShaderStateCompileRequested;
  translator_ = translator;
  if (translator_) {
    options_affecting_compilation_ =
        translator_->GetStringForOptionsThatWouldAffectCompilation();
  }
  source_type_ = type;
  last_compiled_source_ = source_;
}

void Shader::DoCompile() {
  if (!CanCompile()) {
    return;
  }

  // Signify the shader has been compiled, whether or not it is valid
  // is dependent on the |valid_| member variable.
  shader_state_ = kShaderStateCompiled;
  valid_ = false;

  // Translate GL ES 2.0 shader to Desktop GL shader and pass that to
  // glShaderSource and then glCompileShader.
  const char* source_for_driver = last_compiled_source_.c_str();
  ShaderTranslatorInterface* translator = translator_.get();
  if (translator) {
    bool success = translator->Translate(
        last_compiled_source_, &log_info_, &translated_source_,
        &shader_version_, &attrib_map_, &uniform_map_, &varying_map_,
        &interface_block_map_, &output_variable_list_);
    if (!success) {
      return;
    }
    source_for_driver = translated_source_.c_str();
  }

  glShaderSource(service_id_, 1, &source_for_driver, nullptr);
  glCompileShader(service_id_);

  if (source_type_ == kANGLE) {
    RefreshTranslatedShaderSource();
    source_for_driver = translated_source_.c_str();
  }

  GLint status = GL_FALSE;
  glGetShaderiv(service_id_, GL_COMPILE_STATUS, &status);
  if (status == GL_TRUE) {
    valid_ = true;
  } else {
    valid_ = false;

    // We cannot reach here if we are using the shader translator.
    // All invalid shaders must be rejected by the translator.
    // All translated shaders must compile.
    std::string translator_log = log_info_;

    GLint max_len = 0;
    glGetShaderiv(service_id_, GL_INFO_LOG_LENGTH, &max_len);
    log_info_.resize(max_len);
    if (max_len) {
      GLint len = 0;
      glGetShaderInfoLog(service_id_, log_info_.size(), &len, &log_info_.at(0));
      DCHECK(max_len == 0 || len < max_len);
      DCHECK(len == 0 || log_info_[len] == '\0');
      log_info_.resize(len);
    }

    LOG_IF(ERROR, translator)
        << "Shader translator allowed/produced an invalid shader "
        << "unless the driver is buggy:"
        << "\n--Log from shader translator--\n" << translator_log
        << "\n--original-shader--\n" << last_compiled_source_
        << "\n--translated-shader--\n" << source_for_driver
        << "\n--info-log--\n" << log_info_;
  }

  // Translator is no longer required and can be released
  translator_ = nullptr;
}

void Shader::RefreshTranslatedShaderSource() {
  if (source_type_ == kANGLE) {
    GLint max_len = 0;
    glGetShaderiv(service_id_, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
                  &max_len);
    translated_source_.resize(max_len);
    if (max_len) {
      GLint len = 0;
      glGetTranslatedShaderSourceANGLE(service_id_, translated_source_.size(),
                                       &len, &translated_source_.at(0));
      DCHECK(max_len == 0 || len < max_len);
      DCHECK(len == 0 || translated_source_[len] == '\0');
      translated_source_.resize(len);
    }
  }
}

void Shader::MarkForDeletion() {
  DCHECK(!marked_for_deletion_);
  DCHECK_NE(service_id_, 0u);

  marked_for_deletion_ = true;
}

void Shader::DeleteServiceID() {
  if (service_id_) {
    glDeleteShader(service_id_);
    service_id_ = 0;
  }
}

const sh::Attribute* Shader::GetAttribInfo(const std::string& name) const {
  // Vertex attributes can't be arrays or structs (GLSL ES 3.00.4, section
  // 4.3.4, "Input Variables"), so |name| is the top level name used as
  // the AttributeMap key.
  AttributeMap::const_iterator it = attrib_map_.find(name);
  return it != attrib_map_.end() ? &it->second : nullptr;
}

const std::string* Shader::GetAttribMappedName(
    const std::string& original_name) const {
  for (const auto& key_value : attrib_map_) {
    if (key_value.second.name == original_name)
      return &(key_value.first);
  }
  return nullptr;
}

const std::string* Shader::GetUniformMappedName(
    const std::string& original_name) const {
  for (const auto& key_value : uniform_map_) {
    if (key_value.second.name == original_name)
      return &(key_value.first);
  }
  return nullptr;
}

const std::string* Shader::GetVaryingMappedName(
    const std::string& original_name) const {
  for (VaryingMap::const_iterator it = varying_map_.begin();
       it != varying_map_.end(); ++it) {
    if (it->second.name == original_name)
      return &(it->first);
  }
  return nullptr;
}

const std::string* Shader::GetInterfaceBlockMappedName(
    const std::string& original_name) const {
  for (const auto& key_value : interface_block_map_) {
    if (key_value.second.name == original_name)
      return &(key_value.first);
  }
  return nullptr;
}

const std::string* Shader::GetOutputVariableMappedName(
    const std::string& original_name) const {
  for (const auto& value : output_variable_list_) {
    if (value.name == original_name)
      return &value.mappedName;
  }
  return nullptr;
}

const std::string* Shader::GetOriginalNameFromHashedName(
    const std::string& hashed_name) const {
  if (const auto* info = GetAttribInfo(hashed_name)) {
    return &info->name;
  }
  if (const auto* info = GetUniformInfo(hashed_name)) {
    return &info->name;
  }
  if (const auto* info = GetVaryingInfo(hashed_name)) {
    return &info->name;
  }
  if (const auto* info = GetInterfaceBlockInfo(hashed_name)) {
    return &info->name;
  }
  if (const auto* info = GetOutputVariableInfo(hashed_name)) {
    return &info->name;
  }
  return nullptr;
}

const sh::Uniform* Shader::GetUniformInfo(const std::string& name) const {
  UniformMap::const_iterator it = uniform_map_.find(GetTopVariableName(name));
  return it != uniform_map_.end() ? &it->second : nullptr;
}

const sh::Varying* Shader::GetVaryingInfo(const std::string& name) const {
  VaryingMap::const_iterator it = varying_map_.find(GetTopVariableName(name));
  return it != varying_map_.end() ? &it->second : nullptr;
}

const sh::InterfaceBlock* Shader::GetInterfaceBlockInfo(
    const std::string& name) const {
  InterfaceBlockMap::const_iterator it =
      interface_block_map_.find(GetTopVariableName(name));
  return it != interface_block_map_.end() ? &it->second : nullptr;
}

const sh::OutputVariable* Shader::GetOutputVariableInfo(
    const std::string& name) const {
  std::string mapped_name = GetTopVariableName(name);
  // Number of output variables is expected to be so low that
  // a linear search of a list should be faster than using a map.
  for (const auto& value : output_variable_list_) {
    if (value.mappedName == mapped_name)
      return &value;
  }
  return nullptr;
}

ShaderManager::ShaderManager(gl::ProgressReporter* progress_reporter)
    : progress_reporter_(progress_reporter) {}

ShaderManager::~ShaderManager() {
  DCHECK(shaders_.empty());
}

void ShaderManager::Destroy(bool have_context) {
  while (!shaders_.empty()) {
    if (have_context) {
      Shader* shader = shaders_.begin()->second.get();
      shader->Destroy();
    }
    shaders_.erase(shaders_.begin());
    if (progress_reporter_)
      progress_reporter_->ReportProgress();
  }
}

Shader* ShaderManager::CreateShader(
    GLuint client_id,
    GLuint service_id,
    GLenum shader_type) {
  std::pair<ShaderMap::iterator, bool> result =
      shaders_.insert(std::make_pair(
          client_id, scoped_refptr<Shader>(
              new Shader(service_id, shader_type))));
  DCHECK(result.second);
  return result.first->second.get();
}

Shader* ShaderManager::GetShader(GLuint client_id) {
  ShaderMap::iterator it = shaders_.find(client_id);
  return it != shaders_.end() ? it->second.get() : nullptr;
}

bool ShaderManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (ShaderMap::const_iterator it = shaders_.begin();
       it != shaders_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

bool ShaderManager::IsOwned(Shader* shader) {
  for (ShaderMap::iterator it = shaders_.begin();
       it != shaders_.end(); ++it) {
    if (it->second.get() == shader) {
      return true;
    }
  }
  return false;
}

void ShaderManager::RemoveShaderIfUnused(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  if (shader->IsDeleted() && !shader->InUse()) {
    shader->DeleteServiceID();
    for (ShaderMap::iterator it = shaders_.begin();
         it != shaders_.end(); ++it) {
      if (it->second.get() == shader) {
        shaders_.erase(it);
        return;
      }
    }
    NOTREACHED_IN_MIGRATION();
  }
}

void ShaderManager::Delete(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  shader->MarkForDeletion();
  RemoveShaderIfUnused(shader);
}

void ShaderManager::UseShader(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  shader->IncUseCount();
}

void ShaderManager::UnuseShader(Shader* shader) {
  DCHECK(shader);
  DCHECK(IsOwned(shader));
  shader->DecUseCount();
  RemoveShaderIfUnused(shader);
}

}  // namespace gles2
}  // namespace gpu
