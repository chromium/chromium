// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"

using base::TimeDelta;
using base::TimeTicks;

namespace gpu {
namespace gles2 {

namespace {

int ShaderTypeToIndex(GLenum shader_type) {
  switch (shader_type) {
    case GL_VERTEX_SHADER:
      return 0;
    case GL_FRAGMENT_SHADER:
      return 1;
    default:
      NOTREACHED();
      return 0;
  }
}

// Given a name like "foo.bar[123].moo[456]" sets new_name to "foo.bar[123].moo"
// and sets element_index to 456. returns false if element expression was not a
// whole decimal number. For example: "foo[1b2]"
bool GetUniformNameSansElement(
    const std::string& name, int* element_index, std::string* new_name) {
  DCHECK(element_index);
  DCHECK(new_name);
  if (name.size() < 3 || name.back() != ']') {
    *element_index = 0;
    *new_name = name;
    return true;
  }

  // Look for an array specification.
  size_t open_pos = name.find_last_of('[');
  if (open_pos == std::string::npos ||
      open_pos >= name.size() - 2) {
    return false;
  }

  base::CheckedNumeric<GLint> index = 0;
  size_t last = name.size() - 1;
  for (size_t pos = open_pos + 1; pos < last; ++pos) {
    int8_t digit = name[pos] - '0';
    if (digit < 0 || digit > 9) {
      return false;
    }
    index = index * 10 + digit;
  }
  if (!index.IsValid()) {
    return false;
  }

  *element_index = index.ValueOrDie();
  *new_name = name.substr(0, open_pos);
  return true;
}

bool IsBuiltInFragmentVarying(const std::string& name) {
  // Built-in variables for fragment shaders.
  const char* kBuiltInVaryings[] = {
      "gl_FragCoord",
      "gl_FrontFacing",
      "gl_PointCoord"
  };
  for (size_t ii = 0; ii < base::size(kBuiltInVaryings); ++ii) {
    if (name == kBuiltInVaryings[ii])
      return true;
  }
  return false;
}

bool IsBuiltInInvariant(
    const VaryingMap& varyings, const std::string& name) {
  VaryingMap::const_iterator hit = varyings.find(name);
  if (hit == varyings.end())
    return false;
  return hit->second.isInvariant;
}

uint32_t ComputeOffset(const void* start, const void* position) {
  return static_cast<const uint8_t*>(position) -
         static_cast<const uint8_t*>(start);
}

// This is used for vertex shader input variables and fragment shader output
// variables.
ShaderVariableBaseType InputOutputTypeToBaseType(bool is_input, GLenum type) {
  switch (type) {
    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
      return SHADER_VARIABLE_INT;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_VEC2:
    case GL_UNSIGNED_INT_VEC3:
    case GL_UNSIGNED_INT_VEC4:
      return SHADER_VARIABLE_UINT;
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
      return SHADER_VARIABLE_FLOAT;
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2:
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2:
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3:
      DCHECK(is_input);
      return SHADER_VARIABLE_FLOAT;
    default:
      NOTREACHED();
      return SHADER_VARIABLE_UNDEFINED_TYPE;
  }
}

GLsizeiptr VertexShaderOutputBaseTypeToSize(GLenum type) {
  switch (type) {
    case GL_INT:
      return 4;
    case GL_INT_VEC2:
      return 8;
    case GL_INT_VEC3:
      return 12;
    case GL_INT_VEC4:
      return 16;
    case GL_UNSIGNED_INT:
      return 4;
    case GL_UNSIGNED_INT_VEC2:
      return 8;
    case GL_UNSIGNED_INT_VEC3:
      return 12;
    case GL_UNSIGNED_INT_VEC4:
      return 16;
    case GL_FLOAT:
      return 4;
    case GL_FLOAT_VEC2:
      return 8;
    case GL_FLOAT_VEC3:
      return 12;
    case GL_FLOAT_VEC4:
      return 16;
    case GL_FLOAT_MAT2:
      return 16;
    case GL_FLOAT_MAT3:
      return 36;
    case GL_FLOAT_MAT4:
      return 64;
    case GL_FLOAT_MAT2x3:
      return 24;
    case GL_FLOAT_MAT3x2:
      return 24;
    case GL_FLOAT_MAT2x4:
      return 32;
    case GL_FLOAT_MAT4x2:
      return 32;
    case GL_FLOAT_MAT3x4:
      return 48;
    case GL_FLOAT_MAT4x3:
      return 48;
    default:
      NOTREACHED();
      return 0;
  }
}

GLsizeiptr VertexShaderOutputTypeToSize(const sh::Varying& varying) {
  base::CheckedNumeric<GLsizeiptr> total = 0;
  if (varying.fields.size()) {  // struct case.
    for (auto const& field : varying.fields) {
      // struct field for vertex shader outputs can only be basic types.
      GLsizeiptr size = VertexShaderOutputBaseTypeToSize(field.type);
      DCHECK(size);
      total += size;
    }
  } else {
    GLsizeiptr size = VertexShaderOutputBaseTypeToSize(varying.type);
    DCHECK(size);
    total = size;
    if (varying.isArray()) {  // array case.
      total *= varying.getOutermostArraySize();
    }
  }
  return total.ValueOrDefault(std::numeric_limits<GLsizeiptr>::max());
}

size_t LocationCountForAttribType(GLenum type) {
  switch (type) {
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT2x4:
      return 2;
    case GL_FLOAT_MAT3x2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT3x4:
      return 3;
      break;
    case GL_FLOAT_MAT4x2:
    case GL_FLOAT_MAT4x3:
    case GL_FLOAT_MAT4:
      return 4;
    default:
      return 1;
  }
}

}  // anonymous namespace.

Program::UniformInfo::UniformInfo()
    : size(0),
      type(GL_NONE),
      accepts_api_type(UniformApiType::kUniformNone),
      fake_location_base(0),
      is_array(false) {}

Program::UniformInfo::UniformInfo(const std::string& client_name,
                                  int client_location_base,
                                  GLenum _type,
                                  bool _is_array,
                                  const std::vector<GLint>& service_locations)
    : size(service_locations.size()),
      type(_type),
      accepts_api_type(UniformApiType::kUniformNone),
      fake_location_base(client_location_base),
      is_array(_is_array),
      name(client_name),
      element_locations(service_locations) {
  switch (type) {
    case GL_INT:
      accepts_api_type = UniformApiType::kUniform1i;
      break;
    case GL_INT_VEC2:
      accepts_api_type = UniformApiType::kUniform2i;
      break;
    case GL_INT_VEC3:
      accepts_api_type = UniformApiType::kUniform3i;
      break;
    case GL_INT_VEC4:
      accepts_api_type = UniformApiType::kUniform4i;
      break;

    case GL_UNSIGNED_INT:
      accepts_api_type = UniformApiType::kUniform1ui;
      break;
    case GL_UNSIGNED_INT_VEC2:
      accepts_api_type = UniformApiType::kUniform2ui;
      break;
    case GL_UNSIGNED_INT_VEC3:
      accepts_api_type = UniformApiType::kUniform3ui;
      break;
    case GL_UNSIGNED_INT_VEC4:
      accepts_api_type = UniformApiType::kUniform4ui;
      break;

    case GL_BOOL:
      accepts_api_type = UniformApiType::kUniform1i |
                         UniformApiType::kUniform1ui |
                         UniformApiType::kUniform1f;
      break;
    case GL_BOOL_VEC2:
      accepts_api_type = UniformApiType::kUniform2i |
                         UniformApiType::kUniform2ui |
                         UniformApiType::kUniform2f;
      break;
    case GL_BOOL_VEC3:
      accepts_api_type = UniformApiType::kUniform3i |
                         UniformApiType::kUniform3ui |
                         UniformApiType::kUniform3f;
      break;
    case GL_BOOL_VEC4:
      accepts_api_type = UniformApiType::kUniform4i |
                         UniformApiType::kUniform4ui |
                         UniformApiType::kUniform4f;
      break;

    case GL_FLOAT:
      accepts_api_type = UniformApiType::kUniform1f;
      break;
    case GL_FLOAT_VEC2:
      accepts_api_type = UniformApiType::kUniform2f;
      break;
    case GL_FLOAT_VEC3:
      accepts_api_type = UniformApiType::kUniform3f;
      break;
    case GL_FLOAT_VEC4:
      accepts_api_type = UniformApiType::kUniform4f;
      break;

    case GL_FLOAT_MAT2:
      accepts_api_type = UniformApiType::kUniformMatrix2f;
      break;
    case GL_FLOAT_MAT3:
      accepts_api_type = UniformApiType::kUniformMatrix3f;
      break;
    case GL_FLOAT_MAT4:
      accepts_api_type = UniformApiType::kUniformMatrix4f;
      break;

    case GL_FLOAT_MAT2x3:
      accepts_api_type = UniformApiType::kUniformMatrix2x3f;
      break;
    case GL_FLOAT_MAT2x4:
      accepts_api_type = UniformApiType::kUniformMatrix2x4f;
      break;
    case GL_FLOAT_MAT3x2:
      accepts_api_type = UniformApiType::kUniformMatrix3x2f;
      break;
    case GL_FLOAT_MAT3x4:
      accepts_api_type = UniformApiType::kUniformMatrix3x4f;
      break;
    case GL_FLOAT_MAT4x2:
      accepts_api_type = UniformApiType::kUniformMatrix4x2f;
      break;
    case GL_FLOAT_MAT4x3:
      accepts_api_type = UniformApiType::kUniformMatrix4x3f;
      break;

    case GL_SAMPLER_2D:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_3D_OES:
    case GL_SAMPLER_EXTERNAL_OES:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      accepts_api_type = UniformApiType::kUniform1i;
      break;

    default:
      NOTREACHED() << "Unhandled UniformInfo type " << type;
      break;
  }
  DCHECK_LT(0, size);
  DCHECK(is_array || size == 1);

  size_t num_texture_units = IsSampler() ? static_cast<size_t>(size) : 0u;
  texture_units.clear();
  texture_units.resize(num_texture_units, 0);
}

Program::UniformInfo::UniformInfo(const UniformInfo& other) = default;

Program::UniformInfo::~UniformInfo() = default;

bool ProgramManager::HasBuiltInPrefix(const std::string& name) {
  return name.length() >= 3 && name[0] == 'g' && name[1] == 'l' &&
         name[2] == '_';
}

Program::Program(ProgramManager* manager, GLuint service_id)
    : manager_(manager),
      use_count_(0),
      max_attrib_name_length_(0),
      max_uniform_name_length_(0),
      service_id_(service_id),
      deleted_(false),
      valid_(false),
      link_status_(false),
      uniforms_cleared_(false),
      draw_id_uniform_location_(-1),
      base_vertex_uniform_location_(-1),
      base_instance_uniform_location_(-1),
      transform_feedback_buffer_mode_(GL_NONE),
      effective_transform_feedback_buffer_mode_(GL_NONE),
      fragment_output_type_mask_(0u),
      fragment_output_written_mask_(0u) {
  DCHECK(manager_);
  manager_->StartTracking(this);
  uint32_t packed_size = (manager_->max_vertex_attribs() + 15) / 16;
  vertex_input_base_type_mask_.resize(packed_size);
  vertex_input_active_mask_.resize(packed_size);
  ClearVertexInputMasks();
}

void Program::Reset() {
  valid_ = false;
  link_status_ = false;
  max_uniform_name_length_ = 0;
  max_attrib_name_length_ = 0;
  attrib_infos_.clear();
  uniform_infos_.clear();
  uniform_locations_.clear();
  fragment_input_infos_.clear();
  fragment_input_locations_.clear();
  program_output_infos_.clear();
  sampler_indices_.clear();
  attrib_location_to_index_map_.clear();
  fragment_output_type_mask_ = 0u;
  fragment_output_written_mask_ = 0u;
  draw_id_uniform_location_ = -1;
  base_vertex_uniform_location_ = -1;
  base_instance_uniform_location_ = -1;
  ClearVertexInputMasks();
}

void Program::ClearVertexInputMasks() {
  for (uint32_t ii = 0; ii < vertex_input_base_type_mask_.size(); ++ii) {
    vertex_input_base_type_mask_[ii] = 0u;
    vertex_input_active_mask_[ii] = 0u;
  }
}

void Program::UpdateFragmentOutputBaseTypes() {
  fragment_output_type_mask_ = 0u;
  fragment_output_written_mask_ = 0u;
  Shader* fragment_shader =
      shaders_from_last_successful_link_[ShaderTypeToIndex(GL_FRAGMENT_SHADER)]
          .get();
  DCHECK(fragment_shader);
  for (auto const& output : fragment_shader->output_variable_list()) {
    int location = output.location;
    DCHECK(location == -1 ||
           (location >= 0 &&
            location < static_cast<int>(manager_->max_draw_buffers())));
    if (location == -1)
      location = 0;
    if (ProgramManager::HasBuiltInPrefix(output.name)) {
      if (output.name != "gl_FragColor" && output.name != "gl_FragData")
        continue;
    }
    int count =
        static_cast<int>(output.isArray() ? output.getOutermostArraySize() : 1);
    // TODO(zmo): Handle the special case in ES2 where gl_FragColor could
    // be broadcasting to all draw buffers.
    DCHECK_LE(location + count,
              static_cast<int>(manager_->max_draw_buffers()));
    for (int ii = location; ii < location + count; ++ii) {
      // TODO(zmo): This does not work with glBindFragDataLocationIndexed.
      // crbug.com/628010
      // For example:
      //    glBindFragDataLocationIndexed(program, loc, 0, "FragData0");
      //    glBindFragDataLocationIndexed(program, loc, 1, "FragData1");
      // The program links OK, but both calling glGetFragDataLocation on both
      // "FragData0" and "FragData1" returns 0.
      int shift_bits = ii * 2;
      fragment_output_written_mask_ |= 0x3 << shift_bits;
      fragment_output_type_mask_ |=
          InputOutputTypeToBaseType(false, output.type) << shift_bits;
    }
  }
}

void Program::UpdateVertexInputBaseTypes() {
  ClearVertexInputMasks();
  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& input = attrib_infos_[ii];
    if (ProgramManager::HasBuiltInPrefix(input.name)) {
      continue;
    }

    DCHECK_LE(input.location + input.location_count,
              manager_->max_vertex_attribs());
    for (size_t location = input.location;
         location < input.location + input.location_count; ++location) {
      int shift_bits = (location % 16) * 2;
      vertex_input_active_mask_[location / 16] |= 0x3 << shift_bits;
      vertex_input_base_type_mask_[location / 16] |=
          InputOutputTypeToBaseType(true, input.type) << shift_bits;
    }
  }
}

void Program::UpdateUniformBlockSizeInfo() {
  if (feature_info().IsWebGL1OrES2Context()) {
    // Uniform blocks do not exist in ES2.
    return;
  }

  uniform_block_size_info_.clear();

  GLint num_uniform_blocks = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORM_BLOCKS, &num_uniform_blocks);
  uniform_block_size_info_.resize(num_uniform_blocks);
  for (GLint ii = 0; ii < num_uniform_blocks; ++ii) {
    GLint binding = 0;
    glGetActiveUniformBlockiv(
        service_id_, ii, GL_UNIFORM_BLOCK_BINDING, &binding);
    uniform_block_size_info_[ii].binding = static_cast<GLuint>(binding);

    GLint size = 0;
    glGetActiveUniformBlockiv(
        service_id_, ii, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
    uniform_block_size_info_[ii].data_size = static_cast<GLuint>(size);
  }
}

void Program::SetUniformBlockBinding(GLuint index, GLuint binding) {
  DCHECK_GT(uniform_block_size_info_.size(), index);
  uniform_block_size_info_[index].binding = binding;
}

void Program::UpdateTransformFeedbackInfo() {
  effective_transform_feedback_buffer_mode_ = transform_feedback_buffer_mode_;
  effective_transform_feedback_varyings_ = transform_feedback_varyings_;

  Shader* vertex_shader = shaders_from_last_successful_link_[0].get();
  DCHECK(vertex_shader);

  if (effective_transform_feedback_buffer_mode_ == GL_INTERLEAVED_ATTRIBS) {
    transform_feedback_data_size_per_vertex_.resize(1);
  } else {
    transform_feedback_data_size_per_vertex_.resize(
        effective_transform_feedback_varyings_.size());
  }

  base::CheckedNumeric<GLsizeiptr> total = 0;
  for (size_t ii = 0; ii < effective_transform_feedback_varyings_.size();
       ++ii) {
    const std::string& client_name = effective_transform_feedback_varyings_[ii];
    const std::string* service_name =
        vertex_shader->GetVaryingMappedName(client_name);
    DCHECK(service_name);
    const sh::Varying* varying = vertex_shader->GetVaryingInfo(*service_name);
    DCHECK(varying);
    GLsizeiptr size = VertexShaderOutputTypeToSize(*varying);
    DCHECK(size);
    if (effective_transform_feedback_buffer_mode_ == GL_INTERLEAVED_ATTRIBS) {
      total += size;
    } else {
      transform_feedback_data_size_per_vertex_[ii] = size;
    }
  }
  if (effective_transform_feedback_buffer_mode_ == GL_INTERLEAVED_ATTRIBS) {
    transform_feedback_data_size_per_vertex_[0] =
        total.ValueOrDefault(std::numeric_limits<GLsizeiptr>::max());
  }
}

void Program::UpdateDrawIDUniformLocation() {
  DCHECK(IsValid());
  GLint fake_location = GetUniformFakeLocation("gl_DrawID");
  draw_id_uniform_location_ = -1;
  GLint array_index;
  GetUniformInfoByFakeLocation(fake_location, &draw_id_uniform_location_,
                               &array_index);
}

void Program::UpdateBaseVertexUniformLocation() {
  DCHECK(IsValid());
  GLint fake_location = GetUniformFakeLocation("gl_BaseVertex");
  base_vertex_uniform_location_ = -1;
  GLint array_index;
  GetUniformInfoByFakeLocation(fake_location, &base_vertex_uniform_location_,
                               &array_index);
}

void Program::UpdateBaseInstanceUniformLocation() {
  DCHECK(IsValid());
  GLint fake_location = GetUniformFakeLocation("gl_BaseInstance");
  base_instance_uniform_location_ = -1;
  GLint array_index;
  GetUniformInfoByFakeLocation(fake_location, &base_instance_uniform_location_,
                               &array_index);
}

std::string Program::ProcessLogInfo(const std::string& log) {
  std::string output;
  re2::StringPiece input(log);
  std::string prior_log;
  std::string hashed_name;
  while (RE2::Consume(&input,
                      "(.*?)(webgl_[0123456789abcdefABCDEF]+)",
                      &prior_log,
                      &hashed_name)) {
    output += prior_log;

    const std::string* original_name =
        GetOriginalNameFromHashedName(hashed_name);
    if (original_name)
      output += *original_name;
    else
      output += hashed_name;
  }

  return output + input.as_string();
}

void Program::UpdateLogInfo() {
  GLint max_len = 0;
  glGetProgramiv(service_id_, GL_INFO_LOG_LENGTH, &max_len);
  if (max_len == 0) {
    set_log_info(nullptr);
    return;
  }
  std::unique_ptr<char[]> temp(new char[max_len]);
  GLint len = 0;
  glGetProgramInfoLog(service_id_, max_len, &len, temp.get());
  DCHECK(max_len == 0 || len < max_len);
  DCHECK(len == 0 || temp[len] == '\0');
  std::string log(temp.get(), len);
  log = ProcessLogInfo(log);
  set_log_info(log.empty() ? nullptr : log.c_str());
}

void Program::ClearUniforms(std::vector<uint8_t>* zero_buffer) {
  DCHECK(zero_buffer);
  if (uniforms_cleared_) {
    return;
  }
  uniforms_cleared_ = true;
  for (const UniformInfo& uniform_info : uniform_infos_) {
    GLint location = uniform_info.element_locations[0];
    GLsizei size = uniform_info.size;
    uint32_t unit_size =
        GLES2Util::GetElementCountForUniformType(uniform_info.type) *
        GLES2Util::GetElementSizeForUniformType(uniform_info.type);
    DCHECK_LT(0u, unit_size);
    uint32_t size_needed = size * unit_size;
    if (size_needed > zero_buffer->size()) {
      zero_buffer->resize(size_needed, 0u);
    }
    const void* zero = &(*zero_buffer)[0];
    switch (uniform_info.type) {
    case GL_FLOAT:
      glUniform1fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_VEC2:
      glUniform2fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_VEC3:
      glUniform3fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_VEC4:
      glUniform4fv(location, size, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_INT:
    case GL_BOOL:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_EXTERNAL_OES:  // extension.
    case GL_SAMPLER_2D_RECT_ARB:  // extension.
      glUniform1iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_INT_VEC2:
    case GL_BOOL_VEC2:
      glUniform2iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_INT_VEC3:
    case GL_BOOL_VEC3:
      glUniform3iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_INT_VEC4:
    case GL_BOOL_VEC4:
      glUniform4iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_FLOAT_MAT2:
      glUniformMatrix2fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT3:
      glUniformMatrix3fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT4:
      glUniformMatrix4fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;

    // ES3 types.
    case GL_UNSIGNED_INT:
      glUniform1uiv(location, size, reinterpret_cast<const GLuint*>(zero));
      break;
    case GL_SAMPLER_3D:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      glUniform1iv(location, size, reinterpret_cast<const GLint*>(zero));
      break;
    case GL_UNSIGNED_INT_VEC2:
      glUniform2uiv(location, size, reinterpret_cast<const GLuint*>(zero));
      break;
    case GL_UNSIGNED_INT_VEC3:
      glUniform3uiv(location, size, reinterpret_cast<const GLuint*>(zero));
      break;
    case GL_UNSIGNED_INT_VEC4:
      glUniform4uiv(location, size, reinterpret_cast<const GLuint*>(zero));
      break;
    case GL_FLOAT_MAT2x3:
      glUniformMatrix2x3fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT3x2:
      glUniformMatrix3x2fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT2x4:
      glUniformMatrix2x4fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT4x2:
      glUniformMatrix4x2fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT3x4:
      glUniformMatrix3x4fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;
    case GL_FLOAT_MAT4x3:
      glUniformMatrix4x3fv(
          location, size, false, reinterpret_cast<const GLfloat*>(zero));
      break;

    default:
      NOTREACHED();
      break;
    }
  }
}

void Program::Update() {
  Reset();
  UpdateLogInfo();
  link_status_ = true;
  uniforms_cleared_ = false;
  GLint num_attribs = 0;
  GLint max_len = 0;
  size_t num_locations = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  glGetProgramiv(service_id_, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_len);
  // TODO(gman): Should we check for error?
  std::unique_ptr<char[]> name_buffer(new char[max_len]);
  for (GLint ii = 0; ii < num_attribs; ++ii) {
    GLsizei length = 0;
    GLsizei size = 0;
    GLenum type = 0;
    glGetActiveAttrib(
        service_id_, ii, max_len, &length, &size, &type, name_buffer.get());
    DCHECK(max_len == 0 || length < max_len);
    DCHECK(length == 0 || name_buffer[length] == '\0');
    std::string original_name;
    GetVertexAttribData(name_buffer.get(), &original_name, &type);
    base::CheckedNumeric<size_t> location_count = size;
    location_count *= LocationCountForAttribType(type);
    size_t safe_location_count = 0;
    if (!location_count.AssignIfValid(&safe_location_count))
      return;
    GLint location;
    if (base::StartsWith(name_buffer.get(), "gl_",
                         base::CompareCase::SENSITIVE)) {
      // Built-in attributes, for example, gl_VertexID, are still considered
      // as active but their location is -1.
      // However, on MacOSX, drivers return 0 in this case.
      // Set |location| to -1 directly.
      location = -1;
    } else {
      // TODO(gman): Should we check for error?
      location = glGetAttribLocation(service_id_, name_buffer.get());
      base::CheckedNumeric<size_t> max_location = location;
      max_location += safe_location_count;
      size_t safe_max_location = 0;
      if (!max_location.AssignIfValid(&safe_max_location))
        return;
      num_locations = std::max(num_locations, safe_max_location);
    }
    attrib_infos_.push_back(
        VertexAttrib(1, type, original_name, location, safe_location_count));
    max_attrib_name_length_ = std::max(
        max_attrib_name_length_, static_cast<GLsizei>(original_name.size()));
  }

  // Create attrib location to index map.
  attrib_location_to_index_map_.resize(num_locations, -1);
  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    if (info.location < 0)
      continue;
    DCHECK_LE(info.location + info.location_count, num_locations);
    for (size_t j = 0; j < info.location_count; ++j)
      attrib_location_to_index_map_[info.location + j] = ii;
  }

  if (manager_->gpu_preferences_.enable_gpu_service_logging_gpu) {
    DVLOG(1) << "----: attribs for service_id: " << service_id();
    for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
      const VertexAttrib& info = attrib_infos_[ii];
      DVLOG(1) << ii << ": loc = " << info.location
               << ", size = " << info.size
               << ", type = " << GLES2Util::GetStringEnum(info.type)
               << ", name = " << info.name;
    }
  }

  if (!UpdateUniforms())
    return;

  if (manager_->gpu_preferences_.enable_gpu_service_logging_gpu) {
    DVLOG(1) << "----: uniforms for service_id: " << service_id();
    size_t ii = 0;
    for (const UniformInfo& info : uniform_infos_) {
      DVLOG(1) << ii++ << ": loc = " << info.element_locations[0]
               << ", size = " << info.size
               << ", type = " << GLES2Util::GetStringEnum(info.type)
               << ", name = " << info.name;
    }
  }

  UpdateFragmentInputs();
  UpdateProgramOutputs();
  UpdateFragmentOutputBaseTypes();
  UpdateVertexInputBaseTypes();
  UpdateUniformBlockSizeInfo();
  UpdateTransformFeedbackInfo();

  valid_ = true;
}

bool Program::UpdateUniforms() {
  // Reserve each client-bound uniform location. This way unbound uniforms will
  // not be allocated to locations that user expects bound uniforms to be, even
  // if the expected uniforms are optimized away by the driver.
  for (const auto& binding : bind_uniform_location_map_) {
    if (binding.second < 0)
      continue;
    size_t client_location = static_cast<size_t>(binding.second);
    if (uniform_locations_.size() <= client_location)
      uniform_locations_.resize(client_location + 1);
    uniform_locations_[client_location].SetInactive();
  }

  GLint num_uniforms = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  if (num_uniforms <= 0)
    return true;

  uniform_infos_.resize(num_uniforms);

  GLint name_buffer_length = 0;
  glGetProgramiv(service_id_, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                 &name_buffer_length);
  DCHECK(name_buffer_length > 0);
  std::unique_ptr<char[]> name_buffer(new char[name_buffer_length]);

  size_t unused_client_location_cursor = 0;

  for (GLint uniform_index = 0; uniform_index < num_uniforms; ++uniform_index) {
    GLsizei name_length = 0;
    GLsizei size = 0;
    GLenum type = GL_NONE;
    glGetActiveUniform(service_id_, uniform_index, name_buffer_length,
                       &name_length, &size, &type, name_buffer.get());
    // Avoid immediately crashing if glGetActiveUniform misbehaves.
    if (!size)
      return false;
    DCHECK(name_length < name_buffer_length);
    DCHECK(name_length == 0 || name_buffer[name_length] == '\0');
    std::string service_name(name_buffer.get(), name_length);

    GLint service_location = -1;
    // Force builtin uniforms (gl_DepthRange) to have invalid location.
    if (!ProgramManager::HasBuiltInPrefix(service_name)) {
      service_location =
          glGetUniformLocation(service_id_, service_name.c_str());
    }

    // Determine the client name of the uniform and whether it is an array
    // or not.
    bool is_array = false;
    std::string client_name;
    for (size_t i = 0; i < kMaxAttachedShaders && client_name.empty(); ++i) {
      const auto& shader = shaders_from_last_successful_link_[i];
      if (!shader)
        continue;
      const sh::ShaderVariable* info = nullptr;
      const sh::Uniform* uniform = shader->GetUniformInfo(service_name);
      if (uniform &&
          uniform->findInfoByMappedName(service_name, &info, &client_name)) {
        DCHECK(!client_name.empty());
        is_array = info->isArray();
        type = info->type;
        size = std::max(1u, info->getOutermostArraySize());
      } else {
        const InterfaceBlockMap& interface_block_map =
            shader->interface_block_map();
        for (const auto& key_value : interface_block_map) {
          const sh::InterfaceBlock& block = key_value.second;
          bool find = false;
          if (block.instanceName.empty()) {
            for (const auto& value : block.fields) {
              if (value.findInfoByMappedName(service_name, &info,
                      &client_name)) {
                find = true;
                break;
              }
            }
          } else {
            size_t pos = service_name.find_first_of('.');
            std::string top_variable_name = service_name.substr(0, pos);
            if (block.mappedName == top_variable_name) {
              DCHECK(pos != std::string::npos);
              for (const auto& field : block.fields) {
                if (field.findInfoByMappedName(service_name.substr(
                    pos + 1), &info, &client_name)) {
                  find = true;
                  client_name = block.name + "." + client_name;
                  break;
                }
              }
            }
          }
          if (find) {
            DCHECK(!client_name.empty());
            is_array = info->isArray();
            type = info->type;
            size = std::max(1u, info->getOutermostArraySize());
            break;
          }
        }
      }
    }

    if (client_name.empty()) {
      // This happens only in cases where we do not have ANGLE or run unit tests
      // (or ANGLE has a severe bug).
      client_name = service_name;
      GLSLArrayName parsed_service_name(service_name);
      is_array = size > 1 || parsed_service_name.IsArrayName();
    }

    std::string service_base_name = service_name;
    std::string client_base_name = client_name;
    if (is_array) {
      // Some drivers incorrectly return an uniform name of size-1 array without
      // "[0]". In this case, we correct the service name by appending "[0]" to
      // it.
      GLSLArrayName parsed_service_name(service_name);
      if (parsed_service_name.IsArrayName()) {
        service_base_name = parsed_service_name.base_name();
        GLSLArrayName parsed_client_name(client_name);
        client_base_name = parsed_client_name.base_name();
      } else {
        service_name += "[0]";
        client_name += "[0]";
      }
    }

    // Assign a location for the uniform: use either client-bound
    // location or automatically assigned to an unused location.
    size_t client_location_base = 0;
    LocationMap::const_iterator it =
        bind_uniform_location_map_.find(client_base_name);
    if (it != bind_uniform_location_map_.end()) {
      client_location_base = it->second;
    } else {
      while (unused_client_location_cursor < uniform_locations_.size() &&
             !uniform_locations_[unused_client_location_cursor].IsUnused())
        unused_client_location_cursor++;
      if (unused_client_location_cursor == uniform_locations_.size())
        uniform_locations_.resize(unused_client_location_cursor + 1);
      client_location_base = unused_client_location_cursor;
      unused_client_location_cursor++;
    }

    // Populate the uniform list entry.
    std::vector<GLint> service_locations;
    service_locations.resize(size);
    service_locations[0] = service_location;

    if (size > 1) {
      for (GLsizei ii = 1; ii < size; ++ii) {
        std::string element_name(service_base_name + "[" +
                                 base::NumberToString(ii) + "]");
        service_locations[ii] =
            glGetUniformLocation(service_id_, element_name.c_str());
      }
    }

    UniformInfo& info = uniform_infos_[uniform_index];
    info = UniformInfo(client_name, client_location_base, type, is_array,
                       service_locations);
    if (info.IsSampler()) {
      sampler_indices_.push_back(uniform_index);
    }

    // Populate the uniform location list entry.
    // Before linking, we already validated that no two statically used uniforms
    // are bound to the same location.
    DCHECK(!uniform_locations_[client_location_base].IsActive());
    uniform_locations_[client_location_base].SetActive(&info);

    max_uniform_name_length_ = std::max(max_uniform_name_length_,
                                        static_cast<GLsizei>(info.name.size()));
  }
  return true;
}

void Program::UpdateFragmentInputs() {
  if (!feature_info().feature_flags().chromium_path_rendering)
    return;
  for (const auto& binding : bind_fragment_input_location_map_) {
    if (binding.second < 0)
      continue;
    size_t client_location = static_cast<size_t>(binding.second);
    if (fragment_input_locations_.size() <= client_location)
      fragment_input_locations_.resize(client_location + 1);
    fragment_input_locations_[client_location].SetInactive();
  }

  GLint num_fragment_inputs = 0;
  glGetProgramInterfaceiv(service_id_, GL_FRAGMENT_INPUT_NV,
                          GL_ACTIVE_RESOURCES, &num_fragment_inputs);
  if (num_fragment_inputs <= 0)
    return;

  GLint max_len = 0;
  glGetProgramInterfaceiv(service_id_, GL_FRAGMENT_INPUT_NV, GL_MAX_NAME_LENGTH,
                          &max_len);
  DCHECK(max_len > 0);

  std::unique_ptr<char[]> name_buffer(new char[max_len]);

  Shader* fragment_shader =
      shaders_from_last_successful_link_[ShaderTypeToIndex(GL_FRAGMENT_SHADER)]
          .get();

  const GLenum kQueryProperties[] = {GL_LOCATION, GL_TYPE, GL_ARRAY_SIZE};

  std::vector<size_t> client_location_indices;
  for (GLint ii = 0; ii < num_fragment_inputs; ++ii) {
    GLsizei name_length = 0;
    glGetProgramResourceName(service_id_, GL_FRAGMENT_INPUT_NV, ii, max_len,
                             &name_length, name_buffer.get());
    DCHECK(name_length < max_len);
    DCHECK(name_length == 0 || name_buffer[name_length] == '\0');
    // A fragment shader can have gl_FragCoord, gl_FrontFacing or gl_PointCoord
    // built-ins as its input, as well as custom varyings. We are interested in
    // custom varyings, client is allowed to bind only them.
    std::string service_name(name_buffer.get(), name_length);
    if (ProgramManager::HasBuiltInPrefix(service_name))
      continue;
    // Unlike when binding uniforms, we expect the driver to give correct
    // names: "name" for simple variable, "name[0]" for an array.
    GLsizei query_length = 0;
    GLint query_results[base::size(kQueryProperties)] = {
        0,
    };
    glGetProgramResourceiv(service_id_, GL_FRAGMENT_INPUT_NV, ii,
                           base::size(kQueryProperties), kQueryProperties,
                           base::size(query_results), &query_length,
                           query_results);
    DCHECK(query_length == base::size(kQueryProperties));

    GLenum type = static_cast<GLenum>(query_results[1]);
    GLsizei size = static_cast<GLsizei>(query_results[2]);
    std::string client_name;

    const sh::Varying* varying = fragment_shader->GetVaryingInfo(service_name);
    const sh::ShaderVariable* info = nullptr;
    if (varying &&
        varying->findInfoByMappedName(service_name, &info, &client_name)) {
      type = info->type;
      size = std::max(1u, info->getOutermostArraySize());
    } else {
      // Should only happen if there are major bugs in the driver, ANGLE or if
      // the shader translator is disabled.
      DCHECK(feature_info().disable_shader_translator());
      client_name = service_name;
      if (size <= 0)
        continue;
    }

    auto it = bind_fragment_input_location_map_.find(client_name);
    if (it != bind_fragment_input_location_map_.end() && it->second >= 0 &&
        query_results[0] >= 0) {
      size_t client_location = static_cast<size_t>(it->second);
      GLuint service_location = static_cast<GLuint>(query_results[0]);
      fragment_input_infos_.push_back(
          FragmentInputInfo(type, service_location));
      client_location_indices.push_back(client_location);
    }

    if (size <= 1)
      continue;
    GLSLArrayName parsed_client_name(client_name);
    GLSLArrayName parsed_service_name(service_name);
    if (!parsed_client_name.IsArrayName() ||
        parsed_client_name.element_index() != 0 ||
        !parsed_service_name.IsArrayName() ||
        parsed_service_name.element_index() != 0) {
      NOTREACHED() << "GLSL array variable names should end with \"[0]\". "
                      "Likely driver or ANGLE error.";
      continue;
    }

    for (GLsizei jj = 1; jj < size; ++jj) {
      std::string array_spec(std::string("[") + base::NumberToString(jj) + "]");
      std::string client_element_name =
          parsed_client_name.base_name() + array_spec;

      it = bind_fragment_input_location_map_.find(client_element_name);
      if (it != bind_fragment_input_location_map_.end() && it->second >= 0) {
        size_t client_location = static_cast<size_t>(it->second);
        std::string service_element_name =
            parsed_service_name.base_name() + array_spec;
        GLint service_location = glGetProgramResourceLocation(
            service_id_, GL_FRAGMENT_INPUT_NV, service_element_name.c_str());
        if (service_location >= 0) {
          fragment_input_infos_.push_back(
              FragmentInputInfo(type, static_cast<GLuint>(service_location)));
          client_location_indices.push_back(client_location);
        }
      }
    }
  }
  for (size_t i = 0; i < client_location_indices.size(); ++i) {
    size_t client_location = client_location_indices[i];
    // Before linking, we already validated that no two statically used fragment
    // inputs are bound to the same location.
    DCHECK(!fragment_input_locations_[client_location].IsActive());
    fragment_input_locations_[client_location].SetActive(
        &fragment_input_infos_[i]);
  }
}

void Program::UpdateProgramOutputs() {
  if (!feature_info().gl_version_info().is_es3_capable ||
      feature_info().disable_shader_translator())
    return;

  Shader* fragment_shader =
      shaders_from_last_successful_link_[ShaderTypeToIndex(GL_FRAGMENT_SHADER)]
          .get();

  for (auto const& output_var : fragment_shader->output_variable_list()) {
    const std::string& service_name = output_var.mappedName;
    // A fragment shader can have gl_FragColor, gl_SecondaryFragColor, etc
    // built-ins as its output, as well as custom varyings. We are interested
    // only in custom varyings, client is allowed to bind only them.
    if (ProgramManager::HasBuiltInPrefix(service_name))
      continue;

    std::string client_name = output_var.name;
    if (!output_var.isArray()) {
      GLint color_name =
          glGetFragDataLocation(service_id_, service_name.c_str());
      if (color_name < 0)
        continue;
      GLint index = 0;
      if (feature_info().feature_flags().ext_blend_func_extended)
        index = glGetFragDataIndex(service_id_, service_name.c_str());
      if (index < 0)
        continue;
      program_output_infos_.push_back(
          ProgramOutputInfo(color_name, index, client_name));
    } else if (feature_info().workarounds().get_frag_data_info_bug) {
      DCHECK(!feature_info().feature_flags().ext_blend_func_extended);
      GLint color_name =
          glGetFragDataLocation(service_id_, service_name.c_str());
      if (color_name >= 0) {
        GLint index = 0;
        for (size_t ii = 0; ii < output_var.getOutermostArraySize(); ++ii) {
          std::string array_spec(std::string("[") + base::NumberToString(ii) +
                                 "]");
          program_output_infos_.push_back(ProgramOutputInfo(
              color_name + ii, index, client_name + array_spec));
        }
      }
    } else {
      for (size_t ii = 0; ii < output_var.getOutermostArraySize(); ++ii) {
        std::string array_spec(std::string("[") + base::NumberToString(ii) +
                               "]");
        std::string service_element_name(service_name + array_spec);
        GLint color_name =
            glGetFragDataLocation(service_id_, service_element_name.c_str());
        if (color_name < 0)
          continue;
        GLint index = 0;
        if (feature_info().feature_flags().ext_blend_func_extended)
          index = glGetFragDataIndex(service_id_, service_element_name.c_str());
        if (index < 0)
          continue;
        program_output_infos_.push_back(
            ProgramOutputInfo(color_name, index, client_name + array_spec));
      }
    }
  }
}

void Program::ExecuteBindAttribLocationCalls() {
  for (const auto& key_value : bind_attrib_location_map_) {
    const std::string* mapped_name = GetAttribMappedName(key_value.first);
    if (mapped_name)
      glBindAttribLocation(service_id_, key_value.second, mapped_name->c_str());
  }
}

bool Program::ExecuteTransformFeedbackVaryingsCall() {
  if (!transform_feedback_varyings_.empty()) {
    // This is called before program linking, so refer to attached_shaders_.
    Shader* vertex_shader = attached_shaders_[0].get();
    if (!vertex_shader) {
      set_log_info("TransformFeedbackVaryings: missing vertex shader");
      return false;
    }

    std::vector<const char*> mapped_names;
    mapped_names.reserve(transform_feedback_varyings_.size());
    for (StringVector::const_iterator it =
             transform_feedback_varyings_.begin();
         it != transform_feedback_varyings_.end(); ++it) {
      const std::string& orig = *it;
      const std::string* mapped = vertex_shader->GetVaryingMappedName(orig);
      if (!mapped) {
        std::string log = "TransformFeedbackVaryings: no varying named " + orig;
        set_log_info(log.c_str());
        return false;
      }
      mapped_names.push_back(mapped->c_str());
    }
    glTransformFeedbackVaryings(service_id_,
                                mapped_names.size(),
                                &mapped_names.front(),
                                transform_feedback_buffer_mode_);
  }

  return true;
}

void Program::ExecuteProgramOutputBindCalls() {
  if (feature_info().disable_shader_translator()) {
    return;
  }

  // This is called before program linking, so refer to attached_shaders_.
  Shader* fragment_shader =
      attached_shaders_[ShaderTypeToIndex(GL_FRAGMENT_SHADER)].get();
  DCHECK(fragment_shader && fragment_shader->valid());

  if (fragment_shader->shader_version() != 100) {
    // ES SL 1.00 does not have mechanism for introducing variables that could
    // be bound. This means that ES SL 1.00 binding calls would be to
    // non-existing variable names.  Binding calls are only executed with ES SL
    // 3.00 and higher.
    for (auto const& output_var : fragment_shader->output_variable_list()) {
      size_t count = std::max(output_var.getOutermostArraySize(), 1u);
      bool is_array = output_var.isArray();

      for (size_t jj = 0; jj < count; ++jj) {
        std::string name = output_var.name;
        std::string array_spec;
        if (is_array) {
          array_spec = std::string("[") + base::NumberToString(jj) + "]";
          name += array_spec;
        }
        auto it = bind_program_output_location_index_map_.find(name);
        if (it == bind_program_output_location_index_map_.end())
          continue;

        std::string mapped_name = output_var.mappedName;
        if (is_array) {
          mapped_name += array_spec;
        }
        const auto& binding = it->second;
        if (binding.second == 0) {
          // Handles the cases where client called glBindFragDataLocation as
          // well as glBindFragDataLocationIndexed with index == 0.
          glBindFragDataLocation(service_id_, binding.first,
                                 mapped_name.c_str());
        } else {
          DCHECK(feature_info().feature_flags().ext_blend_func_extended);
          glBindFragDataLocationIndexed(service_id_, binding.first,
                                        binding.second, mapped_name.c_str());
        }
      }
    }
    return;
  }

  // Support for EXT_blend_func_extended when used with ES SL 1.00 client
  // shader.

  if (feature_info().gl_version_info().is_es ||
      !feature_info().feature_flags().ext_blend_func_extended)
    return;

  // The underlying context does not support EXT_blend_func_extended
  // natively, need to emulate it.

  // ES SL 1.00 is the only language which contains GLSL built-ins
  // that need to be bound to color indices. If clients use other
  // languages, they also bind the output variables themselves.
  // Map gl_SecondaryFragColorEXT / gl_SecondaryFragDataEXT of
  // EXT_blend_func_extended to real color indexes.
  for (auto const& output_var : fragment_shader->output_variable_list()) {
    const std::string& name = output_var.mappedName;
    if (name == "gl_FragColor") {
      DCHECK_EQ(-1, output_var.location);
      DCHECK_EQ(false, output_var.isArray());
      // We leave these unbound by not giving a binding name. The driver will
      // bind this.
    } else if (name == "gl_FragData") {
      DCHECK_EQ(-1, output_var.location);
      DCHECK_NE(0u, output_var.getOutermostArraySize());
      // We leave these unbound by not giving a binding name. The driver will
      // bind this.
    } else if (name == "gl_SecondaryFragColorEXT") {
      DCHECK_EQ(-1, output_var.location);
      DCHECK_EQ(false, output_var.isArray());
      glBindFragDataLocationIndexed(service_id_, 0, 1,
                                    "angle_SecondaryFragColor");
    } else if (name == "gl_SecondaryFragDataEXT") {
      DCHECK_EQ(-1, output_var.location);
      DCHECK_NE(0u, output_var.getOutermostArraySize());
      glBindFragDataLocationIndexed(service_id_, 0, 1,
                                    "angle_SecondaryFragData");
    }
  }
}

bool Program::Link(ShaderManager* manager,
                   Program::VaryingsPackingOption varyings_packing_option,
                   DecoderClient* client) {
  ClearLinkStatus();

  if (!AttachedShadersExist()) {
    set_log_info("missing shaders");
    return false;
  }

  TimeTicks before_time = TimeTicks::Now();
  bool link = true;
  ProgramCache* cache = manager_->program_cache_;
  // This is called before program linking, so refer to attached_shaders_.
  if (cache &&
      !attached_shaders_[0]->last_compiled_source().empty() &&
      !attached_shaders_[1]->last_compiled_source().empty()) {
    ProgramCache::LinkedProgramStatus status = cache->GetLinkedProgramStatus(
        attached_shaders_[0]->last_compiled_signature(),
        attached_shaders_[1]->last_compiled_signature(),
        &bind_attrib_location_map_,
        transform_feedback_varyings_,
        transform_feedback_buffer_mode_);

    bool cache_hit = status == ProgramCache::LINK_SUCCEEDED;
    UMA_HISTOGRAM_BOOLEAN("GPU.ProgramCache.CacheHit", cache_hit);

    if (cache_hit) {
      ProgramCache::ProgramLoadResult success = cache->LoadLinkedProgram(
          service_id(), attached_shaders_[0].get(), attached_shaders_[1].get(),
          &bind_attrib_location_map_, transform_feedback_varyings_,
          transform_feedback_buffer_mode_, client);
      link = success != ProgramCache::PROGRAM_LOAD_SUCCESS;
      UMA_HISTOGRAM_BOOLEAN("GPU.ProgramCache.LoadBinarySuccess", !link);
    }
  }

  if (link) {
    CompileAttachedShaders();

    if (!CanLink()) {
      set_log_info("invalid shaders");
      return false;
    }
    if (DetectShaderVersionMismatch()) {
      set_log_info("Versions of linked shaders have to match.");
      return false;
    }
    if (DetectAttribLocationBindingConflicts()) {
      set_log_info("glBindAttribLocation() conflicts");
      return false;
    }
    std::string conflicting_name;
    if (DetectUniformsMismatch(&conflicting_name)) {
      std::string info_log = "Uniforms with the same name but different "
                             "type/precision: " + conflicting_name;
      set_log_info(ProcessLogInfo(info_log).c_str());
      return false;
    }
    if (DetectUniformLocationBindingConflicts()) {
      set_log_info("glBindUniformLocationCHROMIUM() conflicts");
      return false;
    }
    if (DetectInterfaceBlocksMismatch(&conflicting_name)) {
      std::string info_log =
          "Interface blocks with the same name but different"
          " fields/layout: " +
          conflicting_name;
      set_log_info(ProcessLogInfo(info_log).c_str());
      return false;
    }
    if (DetectVaryingsMismatch(&conflicting_name)) {
      std::string info_log = "Varyings with the same name but different type, "
                             "or statically used varyings in fragment shader "
                             "are not declared in vertex shader: " +
                             conflicting_name;
      set_log_info(ProcessLogInfo(info_log).c_str());
      return false;
    }
    if (DetectFragmentInputLocationBindingConflicts()) {
      set_log_info("glBindFragmentInputLocationCHROMIUM() conflicts");
      return false;
    }
    if (DetectProgramOutputLocationBindingConflicts()) {
      set_log_info("glBindFragDataLocation() conflicts");
      return false;
    }
    if (DetectBuiltInInvariantConflicts()) {
      set_log_info("Invariant settings for certain built-in varyings "
                   "have to match");
      return false;
    }
    if (DetectGlobalNameConflicts(&conflicting_name)) {
      std::string info_log = "Name conflicts between an uniform and an "
                             "attribute: " + conflicting_name;
      set_log_info(ProcessLogInfo(info_log).c_str());
      return false;
    }
    if (!CheckVaryingsPacking(varyings_packing_option)) {
      set_log_info("Varyings over maximum register limit");
      return false;
    }

    ExecuteBindAttribLocationCalls();
    if (!ExecuteTransformFeedbackVaryingsCall()) {
      return false;
    }

    ExecuteProgramOutputBindCalls();

    if (cache && gl::g_current_gl_driver->ext.b_GL_ARB_get_program_binary) {
      glProgramParameteri(service_id(),
                          PROGRAM_BINARY_RETRIEVABLE_HINT,
                          GL_TRUE);
    }
    glLinkProgram(service_id());
  }

  GLint success = 0;
  glGetProgramiv(service_id(), GL_LINK_STATUS, &success);
  if (success == GL_TRUE) {
    for (size_t ii = 0; ii < kMaxAttachedShaders; ++ii)
      shaders_from_last_successful_link_[ii] = attached_shaders_[ii];
    Update();
    if (link) {
      // Here it makes no difference because attached_shaders_ and
      // shaders_from_last_successful_link_ are still the same.
      // ANGLE updates the translated shader sources on link.
      for (auto shader : attached_shaders_) {
        shader->RefreshTranslatedShaderSource();
      }
      if (cache) {
        cache->SaveLinkedProgram(
            service_id(), attached_shaders_[0].get(),
            attached_shaders_[1].get(), &bind_attrib_location_map_,
            effective_transform_feedback_varyings_,
            effective_transform_feedback_buffer_mode_, client);
      }
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "GPU.ProgramCache.BinaryCacheMissTime",
          static_cast<base::HistogramBase::Sample>(
              (TimeTicks::Now() - before_time).InMicroseconds()),
          1,
          static_cast<base::HistogramBase::Sample>(
              TimeDelta::FromSeconds(10).InMicroseconds()),
          50);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "GPU.ProgramCache.BinaryCacheHitTime",
          static_cast<base::HistogramBase::Sample>(
              (TimeTicks::Now() - before_time).InMicroseconds()),
          1,
          static_cast<base::HistogramBase::Sample>(
              TimeDelta::FromSeconds(1).InMicroseconds()),
          50);
    }
  } else {
    UpdateLogInfo();
  }
  return success == GL_TRUE;
}

void Program::Validate() {
  if (!IsValid()) {
    set_log_info("program not linked");
    return;
  }
  glValidateProgram(service_id());
  UpdateLogInfo();
}

GLint Program::GetUniformFakeLocation(
    const std::string& name) const {
  GLSLArrayName parsed_name(name);

  for (const UniformInfo& info : uniform_infos_) {
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.fake_location_base;
    } else if (parsed_name.IsArrayName() && info.is_array) {
      // Look for an array specification.
      size_t open_pos = info.name.find_last_of('[');
      if (info.name.compare(0, open_pos, parsed_name.base_name()) == 0) {
        int index = parsed_name.element_index();
        if (index < info.size) {
          DCHECK_GT(static_cast<int>(info.element_locations.size()), index);
          if (info.element_locations[index] == -1)
            return -1;
          return ProgramManager::MakeFakeLocation(
              info.fake_location_base, index);
        }
      }
    }
  }
  return -1;
}

GLint Program::GetAttribLocation(
    const std::string& original_name) const {
  for (GLuint ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    if (info.name == original_name) {
      return info.location;
    }
  }
  return -1;
}

const Program::UniformInfo*
    Program::GetUniformInfoByFakeLocation(
        GLint fake_location, GLint* real_location, GLint* array_index) const {
  DCHECK(real_location);
  DCHECK(array_index);
  if (fake_location < 0)
    return nullptr;
  size_t location_index =
      GetUniformLocationIndexFromFakeLocation(fake_location);
  if (location_index >= uniform_locations_.size())
    return nullptr;

  if (!uniform_locations_[location_index].IsActive())
    return nullptr;

  const UniformInfo* info =
      uniform_locations_[location_index].shader_variable();
  size_t element_index = GetArrayElementIndexFromFakeLocation(fake_location);
  if (static_cast<GLsizei>(element_index) >= info->size)
    return nullptr;
  *real_location = info->element_locations[element_index];
  *array_index = element_index;
  return info;
}

bool Program::IsInactiveUniformLocationByFakeLocation(
    GLint fake_location) const {
  if (fake_location < 0)
    return true;
  size_t location_index =
      GetUniformLocationIndexFromFakeLocation(fake_location);
  if (location_index >= uniform_locations_.size())
    return false;
  return uniform_locations_[location_index].IsInactive();
}

const std::string* Program::GetAttribMappedName(
    const std::string& original_name) const {
  // This is called by DetectAttribLocationBindingConflicts() and
  // ExecuteBindAttribLocationCalls(). Both are called before program linking,
  // so refer to attached_shaders_.
  for (auto shader : attached_shaders_) {
    if (shader) {
      const std::string* mapped_name =
          shader->GetAttribMappedName(original_name);
      if (mapped_name)
        return mapped_name;
    }
  }
  return nullptr;
}

const std::string* Program::GetUniformMappedName(
    const std::string& original_name) const {
  // This is called by DetectUniformLocationBindingConflicts(), which is called
  // before program linking, so refer to attached_shaders_.
  for (auto shader : attached_shaders_) {
    if (shader) {
      const std::string* mapped_name =
          shader->GetUniformMappedName(original_name);
      if (mapped_name)
        return mapped_name;
    }
  }
  return nullptr;
}

const std::string* Program::GetOriginalNameFromHashedName(
    const std::string& hashed_name) const {
  // This is called by ProcessLogInfo(), which could in turn be called by
  // UpdateLogInfo(). All these cases are either before program linking, or
  // right after program linking and shader attachments haven't been changed,
  // so refer to attached_shaders_.
  // TODO(zmo): The only exception is for Validate(), for which
  // shaders_from_last_successful_link_ should be used. This is minor issue
  // though, leading to log information from ValidateProgram() could end up
  // with hashed variable names.
  for (auto shader : attached_shaders_) {
    if (shader) {
      const std::string* original_name =
          shader->GetOriginalNameFromHashedName(hashed_name);
      if (original_name)
        return original_name;
    }
  }
  return nullptr;
}

const sh::Varying* Program::GetVaryingInfo(
    const std::string& hashed_name) const {
  for (auto shader : shaders_from_last_successful_link_) {
    if (shader) {
      const sh::Varying* info = shader->GetVaryingInfo(hashed_name);
      if (info)
        return info;
    }
  }
  return nullptr;
}

const sh::InterfaceBlock* Program::GetInterfaceBlockInfo(
    const std::string& hashed_name) const {
  for (auto shader : shaders_from_last_successful_link_) {
    if (shader) {
      const sh::InterfaceBlock* info =
          shader->GetInterfaceBlockInfo(hashed_name);
      if (info)
        return info;
    }
  }
  return nullptr;
}

const Program::FragmentInputInfo* Program::GetFragmentInputInfoByFakeLocation(
    GLint fake_location) const {
  if (fake_location < 0)
    return nullptr;
  size_t location_index = static_cast<size_t>(fake_location);
  if (location_index >= fragment_input_locations_.size())
    return nullptr;
  if (!fragment_input_locations_[location_index].IsActive())
    return nullptr;
  return fragment_input_locations_[location_index].shader_variable();
}

bool Program::IsInactiveFragmentInputLocationByFakeLocation(
    GLint fake_location) const {
  if (fake_location < 0)
    return true;
  size_t location_index = static_cast<size_t>(fake_location);
  if (location_index >= fragment_input_locations_.size())
    return false;
  return fragment_input_locations_[location_index].IsInactive();
}

bool Program::SetUniformLocationBinding(
    const std::string& name, GLint location) {
  std::string short_name;
  int element_index = 0;
  if (!GetUniformNameSansElement(name, &element_index, &short_name) ||
      element_index != 0) {
    return false;
  }
  bind_uniform_location_map_[short_name] = location;
  return true;
}

void Program::SetFragmentInputLocationBinding(const std::string& name,
                                              GLint location) {
  // The client wants to bind either "name" or "name[0]".
  // GL ES 3.1 spec refers to active array names with language such as:
  // "if the string identifies the base name of an active array, where the
  // string would exactly match the name of the variable if the suffix "[0]"
  // were appended to the string".

  // At this point we can not know if the string identifies a simple variable,
  // a base name of an array, or nothing.  Store both, so if user overwrites
  // either, both still work correctly.
  bind_fragment_input_location_map_[name] = location;
  bind_fragment_input_location_map_[name + "[0]"] = location;
}

void Program::SetProgramOutputLocationBinding(const std::string& name,
                                              GLuint color_name) {
  SetProgramOutputLocationIndexedBinding(name, color_name, 0);
}

void Program::SetProgramOutputLocationIndexedBinding(const std::string& name,
                                                     GLuint color_name,
                                                     GLuint index) {
  bind_program_output_location_index_map_[name] =
      std::make_pair(color_name, index);
  bind_program_output_location_index_map_[name + "[0]"] =
      std::make_pair(color_name, index);
}

void Program::GetVertexAttribData(
    const std::string& name, std::string* original_name, GLenum* type) const {
  DCHECK(original_name);
  DCHECK(type);
  Shader* shader =
      shaders_from_last_successful_link_[ShaderTypeToIndex(GL_VERTEX_SHADER)]
          .get();
  if (shader) {
    // Vertex attributes can not be arrays or structs (GLSL ES 3.00.4, section
    // 4.3.4, "Input Variables"), so the top level sh::Attribute returns the
    // information we need.
    const sh::Attribute* info = shader->GetAttribInfo(name);
    if (info) {
      *original_name = info->name;
      *type = info->type;
      return;
    }
  }
  // TODO(zmo): this path should never be reached unless there is a serious
  // bug in the driver or in ANGLE translator.
  *original_name = name;
}

const Program::UniformInfo*
    Program::GetUniformInfo(
        GLint index) const {
  if (static_cast<size_t>(index) >= uniform_infos_.size()) {
    return nullptr;
  }
  return &uniform_infos_[index];
}

bool Program::SetSamplers(
    GLint num_texture_units, GLint fake_location,
    GLsizei count, const GLint* value) {
  // The caller has checked that the location is active and valid.
  DCHECK(fake_location >= 0);
  size_t location_index =
      GetUniformLocationIndexFromFakeLocation(fake_location);
  DCHECK(location_index < uniform_locations_.size());
  DCHECK(uniform_locations_[location_index].IsActive());

  UniformInfo* info = uniform_locations_[location_index].shader_variable();

  size_t element_index = GetArrayElementIndexFromFakeLocation(fake_location);
  if (static_cast<GLsizei>(element_index) >= info->size)
    return true;
  count = std::min(info->size - static_cast<GLsizei>(element_index), count);
  if (info->IsSampler() && count > 0) {
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (value[ii] < 0 || value[ii] >= num_texture_units) {
        return false;
      }
    }
    std::copy(value, value + count,
              info->texture_units.begin() + element_index);
    return true;
  }
  return true;
}

void Program::GetProgramiv(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_ACTIVE_ATTRIBUTES:
      *params = attrib_infos_.size();
      break;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_attrib_name_length_ + 1;
      break;
    case GL_ACTIVE_UNIFORMS:
      *params = uniform_infos_.size();
      break;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = max_uniform_name_length_ + 1;
      break;
    case GL_LINK_STATUS:
      *params = link_status_;
      break;
    case GL_INFO_LOG_LENGTH:
      // Notice +1 to accomodate NULL terminator.
      *params = log_info_.get() ? (log_info_->size() + 1) : 0;
      break;
    case GL_DELETE_STATUS:
      *params = deleted_;
      break;
    case GL_VALIDATE_STATUS:
      if (!IsValid()) {
        *params = GL_FALSE;
      } else {
        glGetProgramiv(service_id_, pname, params);
      }
      break;
    default:
      glGetProgramiv(service_id_, pname, params);
      break;
  }
}

bool Program::AttachShader(
    ShaderManager* shader_manager,
    Shader* shader) {
  DCHECK(shader_manager);
  DCHECK(shader);
  int index = ShaderTypeToIndex(shader->shader_type());
  if (attached_shaders_[index].get() != nullptr) {
    return false;
  }
  attached_shaders_[index] = scoped_refptr<Shader>(shader);
  shader_manager->UseShader(shader);
  return true;
}

bool Program::IsShaderAttached(Shader* shader) {
  return attached_shaders_[ShaderTypeToIndex(shader->shader_type())].get() ==
         shader;
}

void Program::DetachShader(
    ShaderManager* shader_manager,
    Shader* shader) {
  DCHECK(shader_manager);
  DCHECK(shader);
  DCHECK(IsShaderAttached(shader));
  attached_shaders_[ShaderTypeToIndex(shader->shader_type())] = nullptr;
  shader_manager->UnuseShader(shader);
}

void Program::DetachShaders(ShaderManager* shader_manager) {
  DCHECK(shader_manager);
  for (auto shader : attached_shaders_) {
    if (shader) {
      DetachShader(shader_manager, shader.get());
    }
  }
}

void Program::CompileAttachedShaders() {
  for (auto shader : attached_shaders_) {
    if (shader) {
      shader->DoCompile();
    }
  }
}

bool Program::AttachedShadersExist() const {
  for (auto shader : attached_shaders_) {
    if (!shader)
      return false;
  }
  return true;
}

bool Program::CanLink() const {
  for (auto shader : attached_shaders_) {
    if (!shader || !shader->valid()) {
      return false;
    }
  }
  return true;
}

bool Program::DetectShaderVersionMismatch() const {
  int version = Shader::kUndefinedShaderVersion;
  // This is called before program linking, so refer to attached_shaders_.
  for (auto shader : attached_shaders_) {
    if (shader) {
      if (version != Shader::kUndefinedShaderVersion &&
          shader->shader_version() != version) {
        return true;
      }
      version = shader->shader_version();
      DCHECK(version != Shader::kUndefinedShaderVersion);
    }
  }
  return false;
}

bool Program::DetectAttribLocationBindingConflicts() const {
  std::set<GLint> location_binding_used;
  for (const auto& key_value : bind_attrib_location_map_) {
    // Find out if an attribute is statically used in this program's shaders.
    const sh::Attribute* attrib = nullptr;
    const std::string* mapped_name = GetAttribMappedName(key_value.first);
    if (!mapped_name)
      continue;
    for (auto shader : attached_shaders_) {
      if (!shader || !shader->valid())
        continue;
      attrib = shader->GetAttribInfo(*mapped_name);
      if (attrib) {
        if (shader->shader_version() >= 300 || attrib->staticUse)
          break;
        else
          attrib = nullptr;
      }
    }
    if (attrib) {
      size_t num_of_locations = LocationCountForAttribType(attrib->type);
      for (size_t ii = 0; ii < num_of_locations; ++ii) {
        GLint loc = key_value.second + ii;
        auto result = location_binding_used.insert(loc);
        if (!result.second)
          return true;
      }
    }
  }
  return false;
}

bool Program::DetectUniformLocationBindingConflicts() const {
  std::set<GLint> location_binding_used;
  for (auto it : bind_uniform_location_map_) {
    // Find out if an attribute is statically used in this program's shaders.
    const sh::Uniform* uniform = nullptr;
    const std::string* mapped_name = GetUniformMappedName(it.first);
    if (!mapped_name)
      continue;
    for (auto shader : attached_shaders_) {
      if (!shader || !shader->valid())
        continue;
      uniform = shader->GetUniformInfo(*mapped_name);
      if (uniform) {
        if (uniform->staticUse)
          break;
        else
          uniform = nullptr;
      }
    }
    if (uniform) {
      auto result = location_binding_used.insert(it.second);
      if (!result.second)
        return true;
    }
  }
  return false;
}

bool Program::DetectUniformsMismatch(std::string* conflicting_name) const {
  typedef std::map<std::string, const sh::Uniform*> UniformPointerMap;
  UniformPointerMap uniform_pointer_map;
  for (auto shader : attached_shaders_) {
    const UniformMap& shader_uniforms = shader->uniform_map();
    for (const auto& key_value : shader_uniforms) {
      const std::string& name = key_value.first;
      UniformPointerMap::iterator hit = uniform_pointer_map.find(name);
      if (hit == uniform_pointer_map.end()) {
        uniform_pointer_map[name] = &(key_value.second);
      } else {
        // If a uniform is in the map, i.e., it has already been declared by
        // another shader, then the type, precision, etc. must match.
        if (hit->second->isSameUniformAtLinkTime(key_value.second))
          continue;
        *conflicting_name = name;
        return true;
      }
    }
  }
  return false;
}

bool Program::DetectInterfaceBlocksMismatch(
    std::string* conflicting_name) const {
  std::map<std::string, const sh::InterfaceBlock*> interface_pointer_map;
  for (auto shader : attached_shaders_) {
    const InterfaceBlockMap& shader_interfaces = shader->interface_block_map();
    for (const auto& it : shader_interfaces) {
      const auto& name = it.first;
      auto hit = interface_pointer_map.find(name);
      if (hit == interface_pointer_map.end()) {
        interface_pointer_map[name] = &(it.second);
      } else {
        // If an interface is in the map, i.e., it has already been declared by
        // another shader, then the layout must match.
        if (hit->second->isSameInterfaceBlockAtLinkTime(it.second))
          continue;
        *conflicting_name = name;
        return true;
      }
    }
  }
  return false;
}

bool Program::DetectVaryingsMismatch(std::string* conflicting_name) const {
  DCHECK(attached_shaders_[0].get() &&
         attached_shaders_[0]->shader_type() == GL_VERTEX_SHADER &&
         attached_shaders_[1].get() &&
         attached_shaders_[1]->shader_type() == GL_FRAGMENT_SHADER);
  const VaryingMap* vertex_varyings = &(attached_shaders_[0]->varying_map());
  const VaryingMap* fragment_varyings = &(attached_shaders_[1]->varying_map());

  int shader_version = attached_shaders_[0]->shader_version();

  for (const auto& key_value : *fragment_varyings) {
    const std::string& name = key_value.first;
    if (IsBuiltInFragmentVarying(name))
      continue;

    VaryingMap::const_iterator hit = vertex_varyings->find(name);
    if (hit == vertex_varyings->end()) {
      if (key_value.second.staticUse) {
        *conflicting_name = name;
        return true;
      }
      continue;
    }

    if (!hit->second.isSameVaryingAtLinkTime(key_value.second,
                                             shader_version)) {
      *conflicting_name = name;
      return true;
    }
  }
  return false;
}

bool Program::DetectFragmentInputLocationBindingConflicts() const {
  auto* shader = attached_shaders_[ShaderTypeToIndex(GL_FRAGMENT_SHADER)].get();
  if (!shader || !shader->valid())
    return false;

  std::set<GLint> location_binding_used;
  for (auto it : bind_fragment_input_location_map_) {
    // Find out if an fragment input is statically used in this program's
    // shaders.
    const std::string* mapped_name = shader->GetVaryingMappedName(it.first);
    if (!mapped_name)
      continue;
    const sh::Varying* fragment_input = shader->GetVaryingInfo(*mapped_name);
    if (fragment_input && fragment_input->staticUse) {
      auto result = location_binding_used.insert(it.second);
      if (!result.second)
        return true;
    }
  }
  return false;
}

bool Program::DetectProgramOutputLocationBindingConflicts() const {
  if (feature_info().disable_shader_translator()) {
    return false;
  }

  Shader* shader =
      attached_shaders_[ShaderTypeToIndex(GL_FRAGMENT_SHADER)].get();
  DCHECK(shader && shader->valid());

  if (shader->shader_version() == 100)
    return false;

  std::set<LocationIndexMap::mapped_type> location_binding_used;
  for (auto const& output_var : shader->output_variable_list()) {
    if (!output_var.staticUse)
      continue;

    size_t count = std::max(output_var.getOutermostArraySize(), 1u);
    bool is_array = output_var.isArray();

    for (size_t jj = 0; jj < count; ++jj) {
      std::string name = output_var.name;
      if (is_array)
        name += std::string("[") + base::NumberToString(jj) + "]";

      auto it = bind_program_output_location_index_map_.find(name);
      if (it == bind_program_output_location_index_map_.end())
        continue;
      auto result = location_binding_used.insert(it->second);
      if (!result.second)
        return true;
    }
  }
  return false;
}

bool Program::DetectBuiltInInvariantConflicts() const {
  DCHECK(attached_shaders_[0].get() &&
         attached_shaders_[0]->shader_type() == GL_VERTEX_SHADER &&
         attached_shaders_[1].get() &&
         attached_shaders_[1]->shader_type() == GL_FRAGMENT_SHADER);
  const VaryingMap& vertex_varyings = attached_shaders_[0]->varying_map();
  const VaryingMap& fragment_varyings = attached_shaders_[1]->varying_map();

  bool gl_position_invariant = IsBuiltInInvariant(
      vertex_varyings, "gl_Position");
  bool gl_point_size_invariant = IsBuiltInInvariant(
      vertex_varyings, "gl_PointSize");

  bool gl_frag_coord_invariant = IsBuiltInInvariant(
      fragment_varyings, "gl_FragCoord");
  bool gl_point_coord_invariant = IsBuiltInInvariant(
      fragment_varyings, "gl_PointCoord");

  return ((gl_frag_coord_invariant && !gl_position_invariant) ||
          (gl_point_coord_invariant && !gl_point_size_invariant));
}

bool Program::DetectGlobalNameConflicts(std::string* conflicting_name) const {
  DCHECK(attached_shaders_[0].get() &&
         attached_shaders_[0]->shader_type() == GL_VERTEX_SHADER &&
         attached_shaders_[1].get() &&
         attached_shaders_[1]->shader_type() == GL_FRAGMENT_SHADER);
  const UniformMap* uniforms[2];
  uniforms[0] = &(attached_shaders_[0]->uniform_map());
  uniforms[1] = &(attached_shaders_[1]->uniform_map());
  const AttributeMap* attribs =
      &(attached_shaders_[0]->attrib_map());

  for (const auto& key_value : *attribs) {
    for (int ii = 0; ii < 2; ++ii) {
      if (uniforms[ii]->find(key_value.first) != uniforms[ii]->end()) {
        *conflicting_name = key_value.first;
        return true;
      }
    }
  }
  return false;
}

bool Program::CheckVaryingsPacking(
    Program::VaryingsPackingOption option) const {
  DCHECK(attached_shaders_[0].get() &&
         attached_shaders_[0]->shader_type() == GL_VERTEX_SHADER &&
         attached_shaders_[1].get() &&
         attached_shaders_[1]->shader_type() == GL_FRAGMENT_SHADER);
  const VaryingMap* vertex_varyings = &(attached_shaders_[0]->varying_map());
  const VaryingMap* fragment_varyings = &(attached_shaders_[1]->varying_map());

  std::map<std::string, const sh::ShaderVariable*> combined_map;

  for (const auto& key_value : *fragment_varyings) {
    if (!key_value.second.staticUse && option == kCountOnlyStaticallyUsed)
      continue;
    if (!IsBuiltInFragmentVarying(key_value.first)) {
      VaryingMap::const_iterator vertex_iter =
          vertex_varyings->find(key_value.first);
      if (vertex_iter == vertex_varyings->end() ||
          (!vertex_iter->second.staticUse &&
           option == kCountOnlyStaticallyUsed))
        continue;
    }

    combined_map[key_value.first] = &key_value.second;
  }

  if (combined_map.size() == 0)
    return true;
  std::vector<sh::ShaderVariable> variables;
  for (const auto& key_value : combined_map) {
    variables.push_back(*key_value.second);
  }
  return sh::CheckVariablesWithinPackingLimits(
      static_cast<int>(manager_->max_varying_vectors()), variables);
}

void Program::GetProgramInfo(
    ProgramManager* manager, CommonDecoder::Bucket* bucket) const {
  // NOTE: It seems to me the math in here does not need check for overflow
  // because the data being calucated from has various small limits. The max
  // number of attribs + uniforms is somewhere well under 1024. The maximum size
  // of an identifier is 256 characters.
  uint32_t num_locations = 0;
  uint32_t total_string_size = 0;

  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    num_locations += 1;
    total_string_size += info.name.size();
  }

  for (const UniformInfo& info : uniform_infos_) {
    num_locations += info.element_locations.size();
    total_string_size += info.name.size();
  }

  uint32_t num_inputs = attrib_infos_.size() + uniform_infos_.size();
  uint32_t input_size = num_inputs * sizeof(ProgramInput);
  uint32_t location_size = num_locations * sizeof(int32_t);
  uint32_t size = sizeof(ProgramInfoHeader) + input_size + location_size +
                  total_string_size;

  bucket->SetSize(size);
  ProgramInfoHeader* header = bucket->GetDataAs<ProgramInfoHeader*>(0, size);
  ProgramInput* inputs = bucket->GetDataAs<ProgramInput*>(
      sizeof(ProgramInfoHeader), input_size);
  int32_t* locations = bucket->GetDataAs<int32_t*>(
      sizeof(ProgramInfoHeader) + input_size, location_size);
  char* strings = bucket->GetDataAs<char*>(
      sizeof(ProgramInfoHeader) + input_size + location_size,
      total_string_size);
  DCHECK(header);
  DCHECK(inputs);
  DCHECK(locations);
  DCHECK(strings);

  header->link_status = link_status_;
  header->num_attribs = attrib_infos_.size();
  header->num_uniforms = uniform_infos_.size();

  for (size_t ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    inputs->size = info.size;
    inputs->type = info.type;
    inputs->location_offset = ComputeOffset(header, locations);
    inputs->name_offset = ComputeOffset(header, strings);
    inputs->name_length = info.name.size();
    *locations++ = info.location;
    memcpy(strings, info.name.c_str(), info.name.size());
    strings += info.name.size();
    ++inputs;
  }

  for (const UniformInfo& info : uniform_infos_) {
    inputs->size = info.size;
    inputs->type = info.type;
    inputs->location_offset = ComputeOffset(header, locations);
    inputs->name_offset = ComputeOffset(header, strings);
    inputs->name_length = info.name.size();
    DCHECK(static_cast<size_t>(info.size) == info.element_locations.size());
    for (size_t jj = 0; jj < info.element_locations.size(); ++jj) {
      if (info.element_locations[jj] == -1)
        *locations++ = -1;
      else
        *locations++ =
            ProgramManager::MakeFakeLocation(info.fake_location_base, jj);
    }
    memcpy(strings, info.name.c_str(), info.name.size());
    strings += info.name.size();
    ++inputs;
  }
  // NOTE: currently we do not pass inactive uniform binding locations
  // through the program info call.

  // NOTE: currently we do not pass fragment input infos through the program
  // info call, because they are not exposed through any getter function.

  DCHECK_EQ(ComputeOffset(header, strings), size);
}

bool Program::GetUniformBlocks(CommonDecoder::Bucket* bucket) const {
  // The data is packed into the bucket in the following order
  //   1) header
  //   2) N entries of block data (except for name and indices)
  //   3) name1, indices1, name2, indices2, ..., nameN, indicesN
  //
  // We query all the data directly through GL calls, assuming they are
  // cheap through MANGLE.

  DCHECK(bucket);
  GLuint program = service_id();

  uint32_t header_size = sizeof(UniformBlocksHeader);
  bucket->SetSize(header_size);  // In case we fail.

  uint32_t num_uniform_blocks = 0;
  GLint param = GL_FALSE;
  // We assume program is a valid program service id.
  glGetProgramiv(program, GL_LINK_STATUS, &param);
  if (param == GL_TRUE) {
    param = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &param);
    num_uniform_blocks = static_cast<uint32_t>(param);
  }
  if (num_uniform_blocks == 0) {
    // Although spec allows an implementation to return uniform block info
    // even if a link fails, for consistency, we disallow that.
    return true;
  }

  std::vector<UniformBlockInfo> blocks(num_uniform_blocks);
  base::CheckedNumeric<uint32_t> size = sizeof(UniformBlockInfo);
  size *= num_uniform_blocks;
  uint32_t entry_size = size.ValueOrDefault(0);
  size += header_size;
  std::vector<std::string> names(num_uniform_blocks);
  GLint max_name_length = 0;
  glGetProgramiv(
      program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_name_length);
  std::vector<GLchar> buffer(max_name_length);
  GLsizei length;
  for (uint32_t ii = 0; ii < num_uniform_blocks; ++ii) {
    param = 0;
    glGetActiveUniformBlockiv(program, ii, GL_UNIFORM_BLOCK_BINDING, &param);
    blocks[ii].binding = static_cast<uint32_t>(param);

    param = 0;
    glGetActiveUniformBlockiv(program, ii, GL_UNIFORM_BLOCK_DATA_SIZE, &param);
    blocks[ii].data_size = static_cast<uint32_t>(param);

    blocks[ii].name_offset = size.ValueOrDefault(0);
    param = 0;
    glGetActiveUniformBlockiv(
        program, ii, GL_UNIFORM_BLOCK_NAME_LENGTH, &param);
    DCHECK_GE(max_name_length, param);
    memset(&buffer[0], 0, param);
    length = 0;
    glGetActiveUniformBlockName(
        program, ii, static_cast<GLsizei>(param), &length, &buffer[0]);
    DCHECK_EQ(param, length + 1);
    names[ii] = std::string(&buffer[0], length);
    size_t pos = names[ii].find_first_of('[');
    const sh::InterfaceBlock* interface_block = nullptr;
    std::string array_index_str = "";
    if (pos != std::string::npos) {
      interface_block = GetInterfaceBlockInfo(names[ii].substr(0, pos));
      array_index_str = names[ii].substr(pos);
    } else {
      interface_block = GetInterfaceBlockInfo(names[ii]);
    }
    if (interface_block)
      names[ii] = interface_block->name + array_index_str;
    blocks[ii].name_length = names[ii].size() + 1;
    size += blocks[ii].name_length;

    param = 0;
    glGetActiveUniformBlockiv(
        program, ii, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &param);
    blocks[ii].active_uniforms = static_cast<uint32_t>(param);
    blocks[ii].active_uniform_offset = size.ValueOrDefault(0);
    base::CheckedNumeric<uint32_t> indices_size = blocks[ii].active_uniforms;
    indices_size *= sizeof(uint32_t);
    if (!indices_size.IsValid())
      return false;
    size += indices_size.ValueOrDefault(0);

    param = 0;
    glGetActiveUniformBlockiv(
        program, ii, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, &param);
    blocks[ii].referenced_by_vertex_shader = static_cast<uint32_t>(param);

    param = 0;
    glGetActiveUniformBlockiv(
        program, ii, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, &param);
    blocks[ii].referenced_by_fragment_shader = static_cast<uint32_t>(param);
  }
  if (!size.IsValid())
    return false;
  uint32_t total_size = size.ValueOrDefault(0);
  DCHECK_LE(header_size + entry_size, total_size);
  uint32_t data_size = total_size - header_size - entry_size;

  bucket->SetSize(total_size);
  UniformBlocksHeader* header =
      bucket->GetDataAs<UniformBlocksHeader*>(0, header_size);
  UniformBlockInfo* entries = bucket->GetDataAs<UniformBlockInfo*>(
      header_size, entry_size);
  char* data = bucket->GetDataAs<char*>(header_size + entry_size, data_size);
  DCHECK(header);
  DCHECK(entries);
  DCHECK(data);

  // Copy over data for the header and entries.
  header->num_uniform_blocks = num_uniform_blocks;
  memcpy(entries, &blocks[0], entry_size);

  std::vector<GLint> params;
  for (uint32_t ii = 0; ii < num_uniform_blocks; ++ii) {
    // Get active uniform name.
    memcpy(data, names[ii].c_str(), names[ii].length() + 1);
    data += names[ii].length() + 1;

    // Get active uniform indices.
    if (params.size() < blocks[ii].active_uniforms)
      params.resize(blocks[ii].active_uniforms);
    uint32_t num_bytes = blocks[ii].active_uniforms * sizeof(GLint);
    memset(&params[0], 0, num_bytes);
    glGetActiveUniformBlockiv(
        program, ii, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, &params[0]);
    uint32_t* indices = reinterpret_cast<uint32_t*>(data);
    for (uint32_t uu = 0; uu < blocks[ii].active_uniforms; ++uu) {
      indices[uu] = static_cast<uint32_t>(params[uu]);
    }
    data += num_bytes;
  }
  DCHECK_EQ(ComputeOffset(header, data), total_size);
  return true;
}

bool Program::GetTransformFeedbackVaryings(
    CommonDecoder::Bucket* bucket) const {
  // The data is packed into the bucket in the following order
  //   1) header
  //   2) N entries of varying data (except for name)
  //   3) name1, name2, ..., nameN
  //
  // We query all the data directly through GL calls, assuming they are
  // cheap through MANGLE.

  DCHECK(bucket);
  GLuint program = service_id();

  uint32_t header_size = sizeof(TransformFeedbackVaryingsHeader);
  bucket->SetSize(header_size);  // In case we fail.

  GLenum transform_feedback_buffer_mode = 0;
  GLint param = 0;
  glGetProgramiv(program, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, &param);
  transform_feedback_buffer_mode = static_cast<GLenum>(param);

  uint32_t num_transform_feedback_varyings = 0;
  param = GL_FALSE;
  // We assume program is a valid program service id.
  glGetProgramiv(program, GL_LINK_STATUS, &param);
  if (param == GL_TRUE) {
    param = 0;
    glGetProgramiv(program, GL_TRANSFORM_FEEDBACK_VARYINGS, &param);
    num_transform_feedback_varyings = static_cast<uint32_t>(param);
  }
  if (num_transform_feedback_varyings == 0) {
    TransformFeedbackVaryingsHeader* header =
        bucket->GetDataAs<TransformFeedbackVaryingsHeader*>(0, header_size);
    header->transform_feedback_buffer_mode = transform_feedback_buffer_mode;
    return true;
  }

  std::vector<TransformFeedbackVaryingInfo> varyings(
      num_transform_feedback_varyings);
  base::CheckedNumeric<uint32_t> size = sizeof(TransformFeedbackVaryingInfo);
  size *= num_transform_feedback_varyings;
  uint32_t entry_size = size.ValueOrDefault(0);
  size += header_size;
  std::vector<std::string> names(num_transform_feedback_varyings);
  GLint max_name_length = 0;
  glGetProgramiv(
      program, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &max_name_length);
  if (max_name_length < 1)
    max_name_length = 1;
  std::vector<char> buffer(max_name_length);
  for (uint32_t ii = 0; ii < num_transform_feedback_varyings; ++ii) {
    GLsizei var_size = 0;
    GLsizei var_name_length = 0;
    GLenum var_type = 0;
    glGetTransformFeedbackVarying(
        program, ii, max_name_length,
        &var_name_length, &var_size, &var_type, &buffer[0]);
    varyings[ii].size = static_cast<uint32_t>(var_size);
    varyings[ii].type = static_cast<uint32_t>(var_type);
    varyings[ii].name_offset = static_cast<uint32_t>(size.ValueOrDefault(0));
    DCHECK_GT(max_name_length, var_name_length);
    names[ii] = std::string(&buffer[0], var_name_length);
    const sh::Varying* varying = GetVaryingInfo(names[ii]);
    if (varying)
      names[ii] = varying->name;
    varyings[ii].name_length = names[ii].size() + 1;
    size += names[ii].size();
    size += 1;
  }
  if (!size.IsValid())
    return false;
  uint32_t total_size = size.ValueOrDefault(0);
  DCHECK_LE(header_size + entry_size, total_size);
  uint32_t data_size = total_size - header_size - entry_size;

  bucket->SetSize(total_size);
  TransformFeedbackVaryingsHeader* header =
      bucket->GetDataAs<TransformFeedbackVaryingsHeader*>(0, header_size);
  TransformFeedbackVaryingInfo* entries =
      bucket->GetDataAs<TransformFeedbackVaryingInfo*>(header_size, entry_size);
  char* data = bucket->GetDataAs<char*>(header_size + entry_size, data_size);
  DCHECK(header);
  DCHECK(entries);
  DCHECK(data);

  // Copy over data for the header and entries.
  header->transform_feedback_buffer_mode = transform_feedback_buffer_mode;
  header->num_transform_feedback_varyings = num_transform_feedback_varyings;
  memcpy(entries, &varyings[0], entry_size);

  for (uint32_t ii = 0; ii < num_transform_feedback_varyings; ++ii) {
    memcpy(data, names[ii].c_str(), names[ii].length() + 1);
    data += names[ii].length() + 1;
  }
  DCHECK_EQ(ComputeOffset(header, data), total_size);
  return true;
}

bool Program::GetUniformsES3(CommonDecoder::Bucket* bucket) const {
  // The data is packed into the bucket in the following order
  //   1) header
  //   2) N entries of UniformES3Info
  //
  // We query all the data directly through GL calls, assuming they are
  // cheap through MANGLE.

  DCHECK(bucket);
  GLuint program = service_id();

  uint32_t header_size = sizeof(UniformsES3Header);
  bucket->SetSize(header_size);  // In case we fail.

  GLsizei count = 0;
  GLint param = GL_FALSE;
  // We assume program is a valid program service id.
  glGetProgramiv(program, GL_LINK_STATUS, &param);
  if (param == GL_TRUE) {
    param = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
  }
  if (count == 0) {
    return true;
  }

  base::CheckedNumeric<uint32_t> size = sizeof(UniformES3Info);
  size *= count;
  uint32_t entry_size = size.ValueOrDefault(0);
  size += header_size;
  if (!size.IsValid())
    return false;
  uint32_t total_size = size.ValueOrDefault(0);
  bucket->SetSize(total_size);
  UniformsES3Header* header =
      bucket->GetDataAs<UniformsES3Header*>(0, header_size);
  DCHECK(header);
  header->num_uniforms = static_cast<uint32_t>(count);

  // Instead of GetDataAs<UniformES3Info*>, we do GetDataAs<int32_t>. This is
  // because struct UniformES3Info is defined as five int32_t.
  // By doing this, we can fill the structs through loops.
  int32_t* entries =
      bucket->GetDataAs<int32_t*>(header_size, entry_size);
  DCHECK(entries);
  const size_t kStride = sizeof(UniformES3Info) / sizeof(int32_t);

  const GLenum kPname[] = {
    GL_UNIFORM_BLOCK_INDEX,
    GL_UNIFORM_OFFSET,
    GL_UNIFORM_ARRAY_STRIDE,
    GL_UNIFORM_MATRIX_STRIDE,
    GL_UNIFORM_IS_ROW_MAJOR,
  };
  const GLint kDefaultValue[] = { -1, -1, -1, -1, 0 };
  const size_t kNumPnames = base::size(kPname);
  std::vector<GLuint> indices(count);
  for (GLsizei ii = 0; ii < count; ++ii) {
    indices[ii] = ii;
  }
  std::vector<GLint> params(count);
  for (size_t pname_index = 0; pname_index < kNumPnames; ++pname_index) {
    for (GLsizei ii = 0; ii < count; ++ii) {
      params[ii] = kDefaultValue[pname_index];
    }
    glGetActiveUniformsiv(
        program, count, &indices[0], kPname[pname_index], &params[0]);
    for (GLsizei ii = 0; ii < count; ++ii) {
      entries[kStride * ii + pname_index] = params[ii];
    }
  }
  return true;
}

const Program::ProgramOutputInfo* Program::GetProgramOutputInfo(
    const std::string& name) const {
  for (const auto& info : program_output_infos_) {
    if (info.name == name) {
      return &info;
    }
  }
  return nullptr;
}

GLint Program::GetFragDataLocation(const std::string& original_name) const {
  DCHECK(IsValid());
  const ProgramOutputInfo* info = GetProgramOutputInfo(original_name);
  if (!info)
    info = GetProgramOutputInfo(original_name + "[0]");
  if (!info)
    return -1;
  return info->color_name;
}

GLint Program::GetFragDataIndex(const std::string& original_name) const {
  DCHECK(IsValid());
  const ProgramOutputInfo* info = GetProgramOutputInfo(original_name);
  if (!info)
    info = GetProgramOutputInfo(original_name + "[0]");
  if (!info)
    return -1;
  return info->index;
}

void Program::TransformFeedbackVaryings(GLsizei count,
                                        const char* const* varyings,
                                        GLenum buffer_mode) {
  transform_feedback_varyings_.clear();
  for (GLsizei i = 0; i < count; ++i) {
    transform_feedback_varyings_.push_back(std::string(varyings[i]));
  }
  transform_feedback_buffer_mode_ = buffer_mode;
}

Program::~Program() {
  if (manager_) {
    if (manager_->have_context_) {
      glDeleteProgram(service_id());
    }
    manager_->StopTracking(this);
    manager_ = nullptr;
  }
}

ProgramManager::ProgramManager(ProgramCache* program_cache,
                               uint32_t max_varying_vectors,
                               uint32_t max_draw_buffers,
                               uint32_t max_dual_source_draw_buffers,
                               uint32_t max_vertex_attribs,
                               const GpuPreferences& gpu_preferences,
                               FeatureInfo* feature_info,
                               gl::ProgressReporter* progress_reporter)
    : program_count_(0),
      have_context_(true),
      program_cache_(program_cache),
      max_varying_vectors_(max_varying_vectors),
      max_draw_buffers_(max_draw_buffers),
      max_dual_source_draw_buffers_(max_dual_source_draw_buffers),
      max_vertex_attribs_(max_vertex_attribs),
      gpu_preferences_(gpu_preferences),
      feature_info_(feature_info),
      progress_reporter_(progress_reporter) {}

ProgramManager::~ProgramManager() {
  DCHECK(programs_.empty());
}

void ProgramManager::Destroy(bool have_context) {
  have_context_ = have_context;

  while (!programs_.empty()) {
    programs_.erase(programs_.begin());
    if (progress_reporter_)
      progress_reporter_->ReportProgress();
  }
}

void ProgramManager::StartTracking(Program* /* program */) {
  ++program_count_;
}

void ProgramManager::StopTracking(Program* /* program */) {
  --program_count_;
}

Program* ProgramManager::CreateProgram(
    GLuint client_id, GLuint service_id) {
  std::pair<ProgramMap::iterator, bool> result =
      programs_.insert(
          std::make_pair(client_id,
                         scoped_refptr<Program>(
                             new Program(this, service_id))));
  DCHECK(result.second);
  return result.first->second.get();
}

Program* ProgramManager::GetProgram(GLuint client_id) {
  ProgramMap::iterator it = programs_.find(client_id);
  return it != programs_.end() ? it->second.get() : nullptr;
}

bool ProgramManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (const auto& key_value : programs_) {
    if (key_value.second->service_id() == service_id) {
      *client_id = key_value.first;
      return true;
    }
  }
  return false;
}

ProgramCache* ProgramManager::program_cache() const {
  return program_cache_;
}

bool ProgramManager::IsOwned(Program* program) const {
  for (const auto& key_value : programs_) {
    if (key_value.second.get() == program) {
      return true;
    }
  }
  return false;
}

bool ProgramManager::HasCachedCompileStatus(Shader* shader) const {
  if (program_cache_) {
    return program_cache_->HasSuccessfullyCompiledShader(
        shader->last_compiled_signature());
  }
  return false;
}

void ProgramManager::RemoveProgramInfoIfUnused(
    ShaderManager* shader_manager, Program* program) {
  DCHECK(shader_manager);
  DCHECK(program);
  DCHECK(IsOwned(program));
  if (program->IsDeleted() && !program->InUse()) {
    program->DetachShaders(shader_manager);
    for (ProgramMap::iterator it = programs_.begin();
         it != programs_.end(); ++it) {
      if (it->second.get() == program) {
        programs_.erase(it);
        return;
      }
    }
    NOTREACHED();
  }
}

void ProgramManager::MarkAsDeleted(
    ShaderManager* shader_manager,
    Program* program) {
  DCHECK(shader_manager);
  DCHECK(program);
  DCHECK(IsOwned(program));
  program->MarkAsDeleted();
  RemoveProgramInfoIfUnused(shader_manager, program);
}

void ProgramManager::UseProgram(Program* program) {
  DCHECK(program);
  DCHECK(IsOwned(program));
  program->IncUseCount();
}

void ProgramManager::UnuseProgram(
    ShaderManager* shader_manager,
    Program* program) {
  DCHECK(shader_manager);
  DCHECK(program);
  DCHECK(IsOwned(program));
  program->DecUseCount();
  RemoveProgramInfoIfUnused(shader_manager, program);
}

void ProgramManager::ClearUniforms(Program* program) {
  DCHECK(program);
  program->ClearUniforms(&zero_);
}

void ProgramManager::UpdateDrawIDUniformLocation(Program* program) {
  DCHECK(program);
  program->UpdateDrawIDUniformLocation();
}

void ProgramManager::UpdateBaseVertexUniformLocation(Program* program) {
  DCHECK(program);
  program->UpdateBaseVertexUniformLocation();
}

void ProgramManager::UpdateBaseInstanceUniformLocation(Program* program) {
  DCHECK(program);
  program->UpdateBaseInstanceUniformLocation();
}

int32_t ProgramManager::MakeFakeLocation(int32_t index, int32_t element) {
  return index + element * 0x10000;
}

}  // namespace gles2
}  // namespace gpu
