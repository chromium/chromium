// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/program_info_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string_view>

#include "base/memory/raw_span.h"

namespace {

// Loads a value from type `T` from `data` at `offset`. The location need not be
// aligned for `T`.
template <typename T>
T Load(base::span<const int8_t> data, size_t offset) {
  auto subspan = data.subspan(offset, sizeof(T));
  T ret;
  memcpy(&ret, subspan.data(), sizeof(T));
  return ret;
}

std::string_view ToStringView(base::span<const int8_t> data) {
  return std::string_view(reinterpret_cast<const char*>(data.data()),
                          data.size());
}

// A convenience class to load a series of objects and spans.
class DataIterator {
 public:
  explicit DataIterator(base::span<const int8_t> data) : data_(data) {}

  bool empty() const { return data_.empty(); }

  // Consumes `n` bytes and returns them.
  base::span<const int8_t> GetBytes(size_t n) {
    base::span<const int8_t> ret = data_.first(n);
    data_ = data_.subspan(n);
    return ret;
  }

  // Consumes `sizeof(T)` bytes and interprets the result as a `T`.
  template <typename T>
  T Get() {
    return Load<T>(GetBytes(sizeof(T)), 0);
  }

 private:
  base::raw_span<const int8_t> data_;
};

// Writes the string pointed by name and of maximum size buffsize. If length is
// !null, it receives the number of characters written (excluding the final \0).
// This is a helper function for GetActive*Helper functions that return names.
void FillNameAndLength(GLsizei bufsize,
                       GLsizei* length,
                       char* name,
                       const std::string& string) {
  // Length of string (without final \0) that we will write to the
  // buffer.
  GLsizei max_length = 0;
  if (name && (bufsize > 0)) {
    DCHECK_LE(string.size(), static_cast<size_t>(INT_MAX));
    // Note: bufsize counts the terminating \0, but not string.size().
    max_length = std::min(bufsize - 1, static_cast<GLsizei>(string.size()));
    memcpy(name, string.data(), max_length);
    name[max_length] = '\0';
  }
  if (length) {
    *length = max_length;
  }
}

}  // namespace

namespace gpu {
namespace gles2 {

ProgramInfoManager::Program::VertexAttrib::VertexAttrib(
    GLsizei _size, GLenum _type, const std::string& _name, GLint _location)
    : size(_size),
      type(_type),
      location(_location),
      name(_name) {
}

ProgramInfoManager::Program::VertexAttrib::~VertexAttrib() = default;

ProgramInfoManager::Program::UniformInfo::UniformInfo(
    GLsizei _size, GLenum _type, const std::string& _name)
    : size(_size),
      type(_type),
      name(_name) {
  is_array = (!name.empty() && name.back() == ']');
  DCHECK(!(size > 1 && !is_array));
}

ProgramInfoManager::Program::UniformInfo::UniformInfo(
    const UniformInfo& other) = default;

ProgramInfoManager::Program::UniformInfo::~UniformInfo() = default;

ProgramInfoManager::Program::UniformES3::UniformES3()
    : block_index(-1),
      offset(-1),
      array_stride(-1),
      matrix_stride(-1),
      is_row_major(0) {
}

ProgramInfoManager::Program::UniformES3::~UniformES3() = default;

ProgramInfoManager::Program::UniformBlock::UniformBlock()
    : binding(0),
      data_size(0),
      referenced_by_vertex_shader(false),
      referenced_by_fragment_shader(false) {
}

ProgramInfoManager::Program::UniformBlock::UniformBlock(
    const UniformBlock& other) = default;

ProgramInfoManager::Program::UniformBlock::~UniformBlock() = default;

ProgramInfoManager::Program::TransformFeedbackVarying::
TransformFeedbackVarying()
    : size(0),
      type(0) {
}

ProgramInfoManager::Program::TransformFeedbackVarying::
    ~TransformFeedbackVarying() = default;

ProgramInfoManager::Program::Program()
    : cached_es2_(false),
      max_attrib_name_length_(0),
      max_uniform_name_length_(0),
      link_status_(false),
      cached_es3_uniform_blocks_(false),
      active_uniform_block_max_name_length_(0),
      cached_es3_transform_feedback_varyings_(false),
      transform_feedback_varying_max_length_(0),
      transform_feedback_buffer_mode_(0),
      cached_es3_uniformsiv_(false) {
}

ProgramInfoManager::Program::Program(const Program& other) = default;

ProgramInfoManager::Program::~Program() = default;

// TODO(gman): Add a faster lookup.
GLint ProgramInfoManager::Program::GetAttribLocation(
    const std::string& name) const {
  for (GLuint ii = 0; ii < attrib_infos_.size(); ++ii) {
    const VertexAttrib& info = attrib_infos_[ii];
    if (info.name == name) {
      return info.location;
    }
  }
  return -1;
}

const ProgramInfoManager::Program::VertexAttrib*
ProgramInfoManager::Program::GetAttribInfo(GLint index) const {
  return (static_cast<size_t>(index) < attrib_infos_.size())
             ? &attrib_infos_[index]
             : nullptr;
}

const ProgramInfoManager::Program::UniformInfo*
ProgramInfoManager::Program::GetUniformInfo(GLint index) const {
  return (static_cast<size_t>(index) < uniform_infos_.size())
             ? &uniform_infos_[index]
             : nullptr;
}

const ProgramInfoManager::Program::UniformBlock*
ProgramInfoManager::Program::GetUniformBlock(GLuint index) const {
  return (index < uniform_blocks_.size()) ? &uniform_blocks_[index] : nullptr;
}

GLint ProgramInfoManager::Program::GetUniformLocation(
    const std::string& name) const {
  GLSLArrayName parsed_name(name);

  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return info.element_locations[0];
    } else if (parsed_name.IsArrayName() && info.is_array) {
      // Look for an array specification.
      size_t open_pos = info.name.find_last_of('[');
      if (info.name.compare(0, open_pos, parsed_name.base_name()) == 0) {
        int index = parsed_name.element_index();
        if (index < info.size) {
          return info.element_locations[index];
        }
      }
    }
  }
  return -1;
}

GLuint ProgramInfoManager::Program::GetUniformIndex(
    const std::string& name) const {
  // TODO(zmo): Maybe build a hashed_map for faster lookup.
  for (GLuint ii = 0; ii < uniform_infos_.size(); ++ii) {
    const UniformInfo& info = uniform_infos_[ii];
    // For an array, either "var" or "var[0]" is considered as a match.
    // See "OpenGL ES 3.0.0, Section 2.11.3 Program Objects."
    if (info.name == name ||
        (info.is_array &&
         info.name.compare(0, info.name.size() - 3, name) == 0)) {
      return ii;
    }
  }
  return GL_INVALID_INDEX;
}

GLint ProgramInfoManager::Program::GetFragDataIndex(
    const std::string& name) const {
  auto iter = frag_data_indices_.find(name);
  if (iter == frag_data_indices_.end())
    return -1;
  return iter->second;
}

void ProgramInfoManager::Program::CacheFragDataIndex(const std::string& name,
                                                     GLint index) {
  frag_data_indices_[name] = index;
}

GLint ProgramInfoManager::Program::GetFragDataLocation(
    const std::string& name) const {
  std::unordered_map<std::string, GLint>::const_iterator iter =
      frag_data_locations_.find(name);
  if (iter == frag_data_locations_.end())
    return -1;
  return iter->second;
}

void ProgramInfoManager::Program::CacheFragDataLocation(
    const std::string& name, GLint loc) {
  frag_data_locations_[name] = loc;
}

bool ProgramInfoManager::Program::GetProgramiv(
    GLenum pname, GLint* params) {
  switch (pname) {
    case GL_LINK_STATUS:
      *params = static_cast<GLint>(link_status_);
      return true;
    case GL_ACTIVE_ATTRIBUTES:
      *params = static_cast<GLint>(attrib_infos_.size());
      return true;
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      *params = static_cast<GLint>(max_attrib_name_length_);
      return true;
    case GL_ACTIVE_UNIFORMS:
      *params = static_cast<GLint>(uniform_infos_.size());
      return true;
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      *params = static_cast<GLint>(max_uniform_name_length_);
      return true;
    case GL_ACTIVE_UNIFORM_BLOCKS:
      *params = static_cast<GLint>(uniform_blocks_.size());
      return true;
    case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
      *params = static_cast<GLint>(active_uniform_block_max_name_length_);
      return true;
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
      *params = static_cast<GLint>(transform_feedback_varyings_.size());
      return true;
    case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
      *params = static_cast<GLint>(transform_feedback_varying_max_length_);
      return true;
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
      *params = static_cast<GLint>(transform_feedback_buffer_mode_);
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return false;
}

GLuint ProgramInfoManager::Program::GetUniformBlockIndex(
    const std::string& name) const {
  for (size_t ii = 0; ii < uniform_blocks_.size(); ++ii) {
    if (uniform_blocks_[ii].name == name) {
      return static_cast<GLuint>(ii);
    }
  }
  return GL_INVALID_INDEX;
}

void ProgramInfoManager::Program::UniformBlockBinding(
    GLuint index , GLuint binding) {
  if (index < uniform_blocks_.size()) {
    uniform_blocks_[index].binding = binding;
  }
}

const ProgramInfoManager::Program::TransformFeedbackVarying*
ProgramInfoManager::Program::GetTransformFeedbackVarying(GLuint index) const {
  return (index < transform_feedback_varyings_.size())
             ? &transform_feedback_varyings_[index]
             : nullptr;
}

bool ProgramInfoManager::Program::GetUniformsiv(
    GLsizei count, const GLuint* indices, GLenum pname, GLint* params) {
  if (count == 0) {
    // At this point, pname has already been validated.
    return true;
  }
  DCHECK(count > 0 && indices);
  size_t num_uniforms = uniform_infos_.size();
  if (num_uniforms == 0) {
    num_uniforms = uniforms_es3_.size();
  }
  if (static_cast<size_t>(count) > num_uniforms) {
    return false;
  }
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (indices[ii] >= num_uniforms) {
      return false;
    }
  }
  if (!params) {
    return true;
  }
  switch (pname) {
    case GL_UNIFORM_SIZE:
      DCHECK_EQ(num_uniforms, uniform_infos_.size());
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = static_cast<GLint>(uniform_infos_[indices[ii]].size);
      }
      return true;
    case GL_UNIFORM_TYPE:
      DCHECK_EQ(num_uniforms, uniform_infos_.size());
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = static_cast<GLint>(uniform_infos_[indices[ii]].type);
      }
      return true;
    case GL_UNIFORM_NAME_LENGTH:
      DCHECK_EQ(num_uniforms, uniform_infos_.size());
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = static_cast<GLint>(
            uniform_infos_[indices[ii]].name.length() + 1);
      }
      return true;
  }
  if (num_uniforms != uniforms_es3_.size()) {
    return false;
  }
  switch (pname) {
    case GL_UNIFORM_BLOCK_INDEX:
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = uniforms_es3_[indices[ii]].block_index;
      }
      return true;
    case GL_UNIFORM_OFFSET:
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = uniforms_es3_[indices[ii]].offset;
      }
      return true;
    case GL_UNIFORM_ARRAY_STRIDE:
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = uniforms_es3_[indices[ii]].array_stride;
      }
      return true;
    case GL_UNIFORM_MATRIX_STRIDE:
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = uniforms_es3_[indices[ii]].matrix_stride;
      }
      return true;
    case GL_UNIFORM_IS_ROW_MAJOR:
      for (GLsizei ii = 0; ii < count; ++ii) {
        params[ii] = uniforms_es3_[indices[ii]].is_row_major;
      }
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return false;
}

void ProgramInfoManager::Program::UpdateES2(base::span<const int8_t> result) {
  if (cached_es2_) {
    return;
  }
  if (result.empty()) {
    // This should only happen on a lost context.
    return;
  }
  auto header = Load<ProgramInfoHeader>(result, 0);
  link_status_ = header.link_status != 0;
  if (!link_status_) {
    return;
  }
  DCHECK_EQ(0u, attrib_infos_.size());
  DCHECK_EQ(0u, uniform_infos_.size());
  DCHECK_EQ(0, max_attrib_name_length_);
  DCHECK_EQ(0, max_uniform_name_length_);
  DataIterator inputs(result.subspan(
      sizeof(header),
      sizeof(ProgramInput) * (header.num_attribs + header.num_uniforms)));
  for (uint32_t ii = 0; ii < header.num_attribs; ++ii) {
    auto input = inputs.Get<ProgramInput>();
    uint32_t location = Load<uint32_t>(result, input.location_offset);
    std::string name(
        ToStringView(result.subspan(input.name_offset, input.name_length)));
    attrib_infos_.push_back(
        VertexAttrib(input.size, input.type, name, location));
    max_attrib_name_length_ = std::max(
        static_cast<GLsizei>(name.size() + 1), max_attrib_name_length_);
  }
  for (uint32_t ii = 0; ii < header.num_uniforms; ++ii) {
    auto input = inputs.Get<ProgramInput>();
    DataIterator locations(
        result.subspan(input.location_offset, sizeof(int32_t) * input.size));
    std::string name(
        ToStringView(result.subspan(input.name_offset, input.name_length)));
    UniformInfo info(input.size, input.type, name);
    max_uniform_name_length_ = std::max(
        static_cast<GLsizei>(name.size() + 1), max_uniform_name_length_);
    for (int32_t jj = 0; jj < input.size; ++jj) {
      info.element_locations.push_back(locations.Get<uint32_t>());
    }
    uniform_infos_.push_back(info);
  }
  DCHECK(inputs.empty());
  cached_es2_ = true;
}

void ProgramInfoManager::Program::UpdateES3UniformBlocks(
    base::span<const int8_t> result) {
  if (cached_es3_uniform_blocks_) {
    return;
  }
  if (result.empty()) {
    // This should only happen on a lost context.
    return;
  }
  DCHECK_EQ(0u, uniform_blocks_.size());
  DCHECK_EQ(0u, active_uniform_block_max_name_length_);

  // |result| comes from GPU process. We consider it trusted data. Therefore,
  // no need to check for overflows as the GPU side did the checks already.
  uint32_t header_size = sizeof(UniformBlocksHeader);
  DCHECK_GE(result.size(), header_size);
  UniformBlocksHeader header = Load<UniformBlocksHeader>(result, 0);
  if (header.num_uniform_blocks == 0) {
    DCHECK_EQ(result.size(), header_size);
    // TODO(zmo): Here we can't tell if no uniform blocks are defined, or
    // the previous link failed.
    return;
  }
  uniform_blocks_.resize(header.num_uniform_blocks);

  uint32_t entry_size = sizeof(UniformBlockInfo) * header.num_uniform_blocks;
  DCHECK_GE(result.size(), header_size + entry_size);
  DataIterator entries(result.subspan(header_size, entry_size));
  DataIterator data(result.subspan(header_size + entry_size));

  for (uint32_t ii = 0; ii < header.num_uniform_blocks; ++ii) {
    auto entry = entries.Get<UniformBlockInfo>();
    uniform_blocks_[ii].binding = static_cast<GLuint>(entry.binding);
    uniform_blocks_[ii].data_size = static_cast<GLuint>(entry.data_size);
    uniform_blocks_[ii].active_uniform_indices.resize(entry.active_uniforms);
    uniform_blocks_[ii].referenced_by_vertex_shader =
        static_cast<GLboolean>(entry.referenced_by_vertex_shader);
    uniform_blocks_[ii].referenced_by_fragment_shader =
        static_cast<GLboolean>(entry.referenced_by_fragment_shader);
    // Uniform block names can't be empty strings.
    DCHECK_LT(1u, entry.name_length);
    if (entry.name_length > active_uniform_block_max_name_length_) {
      active_uniform_block_max_name_length_ = entry.name_length;
    }
    base::span<const int8_t> name = data.GetBytes(entry.name_length);
    uniform_blocks_[ii].name = ToStringView(name.first(entry.name_length - 1));
    for (uint32_t uu = 0; uu < entry.active_uniforms; ++uu) {
      uniform_blocks_[ii].active_uniform_indices[uu] =
          static_cast<GLuint>(data.Get<uint32_t>());
    }
  }
  DCHECK(data.empty());
  cached_es3_uniform_blocks_ = true;
}

void ProgramInfoManager::Program::UpdateES3Uniformsiv(
    base::span<const int8_t> result) {
  if (cached_es3_uniformsiv_) {
    return;
  }
  if (result.empty()) {
    // This should only happen on a lost context.
    return;
  }
  DCHECK_EQ(0u, uniforms_es3_.size());

  // |result| comes from GPU process. We consider it trusted data. Therefore,
  // no need to check for overflows as the GPU side did the checks already.
  uint32_t header_size = sizeof(UniformsES3Header);
  DCHECK_GE(result.size(), header_size);
  auto header = Load<UniformsES3Header>(result, 0);
  if (header.num_uniforms == 0) {
    DCHECK_EQ(result.size(), header_size);
    // TODO(zmo): Here we can't tell if no uniforms are defined, or
    // the previous link failed.
    return;
  }
  uniforms_es3_.resize(header.num_uniforms);

  uint32_t entry_size = sizeof(UniformES3Info) * header.num_uniforms;
  DCHECK_EQ(result.size(), header_size + entry_size);
  DataIterator entries(result.subspan(header_size, entry_size));

  for (uint32_t ii = 0; ii < header.num_uniforms; ++ii) {
    auto entry = entries.Get<UniformES3Info>();
    uniforms_es3_[ii].block_index = entry.block_index;
    uniforms_es3_[ii].offset = entry.offset;
    uniforms_es3_[ii].array_stride = entry.array_stride;
    uniforms_es3_[ii].matrix_stride = entry.matrix_stride;
    uniforms_es3_[ii].is_row_major = entry.is_row_major;
  }
  DCHECK(entries.empty());
  cached_es3_uniformsiv_ = true;
}

void ProgramInfoManager::Program::UpdateES3TransformFeedbackVaryings(
    base::span<const int8_t> result) {
  if (cached_es3_transform_feedback_varyings_) {
    return;
  }
  if (result.empty()) {
    // This should only happen on a lost context.
    return;
  }
  DCHECK_EQ(0u, transform_feedback_buffer_mode_);
  DCHECK_EQ(0u, transform_feedback_varyings_.size());
  DCHECK_EQ(0u, transform_feedback_varying_max_length_);

  // |result| comes from GPU process. We consider it trusted data. Therefore,
  // no need to check for overflows as the GPU side did the checks already.
  uint32_t header_size = sizeof(TransformFeedbackVaryingsHeader);
  DCHECK_GE(result.size(), header_size);
  auto header = Load<TransformFeedbackVaryingsHeader>(result, 0);
  if (header.num_transform_feedback_varyings == 0) {
    DCHECK_EQ(result.size(), header_size);
    // TODO(zmo): Here we can't tell if no TransformFeedback varyings are
    // defined, or the previous link failed.
    return;
  }
  transform_feedback_varyings_.resize(header.num_transform_feedback_varyings);
  transform_feedback_buffer_mode_ = header.transform_feedback_buffer_mode;

  uint32_t entry_size = sizeof(TransformFeedbackVaryingInfo) *
                        header.num_transform_feedback_varyings;
  DataIterator entries(result.subspan(header_size, entry_size));
  DataIterator data(result.subspan(header_size + entry_size));

  for (uint32_t ii = 0; ii < header.num_transform_feedback_varyings; ++ii) {
    auto entry = entries.Get<TransformFeedbackVaryingInfo>();
    transform_feedback_varyings_[ii].size = static_cast<GLsizei>(entry.size);
    transform_feedback_varyings_[ii].type = static_cast<GLenum>(entry.type);
    DCHECK_LE(1u, entry.name_length);
    if (entry.name_length > transform_feedback_varying_max_length_) {
      transform_feedback_varying_max_length_ = entry.name_length;
    }
    base::span<const int8_t> name = data.GetBytes(entry.name_length);
    transform_feedback_varyings_[ii].name =
        ToStringView(name.first(entry.name_length - 1));
  }
  DCHECK(data.empty());
  cached_es3_transform_feedback_varyings_ = true;
}

bool ProgramInfoManager::Program::IsCached(ProgramInfoType type) const {
  switch (type) {
    case kES2:
      return cached_es2_;
    case kES3UniformBlocks:
      return cached_es3_uniform_blocks_;
    case kES3TransformFeedbackVaryings:
      return cached_es3_transform_feedback_varyings_;
    case kES3Uniformsiv:
      return cached_es3_uniformsiv_;
    case kNone:
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return true;
  }
}

ProgramInfoManager::ProgramInfoManager() = default;

ProgramInfoManager::~ProgramInfoManager() = default;

ProgramInfoManager::Program* ProgramInfoManager::GetProgramInfo(
    GLES2Implementation* gl,
    GLuint program,
    ProgramInfoType type) {
  ProgramInfoMap::iterator it = program_infos_.find(program);
  if (it == program_infos_.end()) {
    return nullptr;
  }
  Program* info = &it->second;
  if (info->IsCached(type))
    return info;

  std::vector<int8_t> result;
  switch (type) {
    case kES2:
      {
        base::AutoUnlock unlock(lock_);
        // lock_ can't be held across IPC call or else it may deadlock in
        // pepper. http://crbug.com/418651
        gl->GetProgramInfoCHROMIUMHelper(program, &result);
      }
      info->UpdateES2(result);
      break;
    case kES3UniformBlocks:
      {
        base::AutoUnlock unlock(lock_);
        // lock_ can't be held across IPC call or else it may deadlock in
        // pepper. http://crbug.com/418651
        gl->GetUniformBlocksCHROMIUMHelper(program, &result);
      }
      info->UpdateES3UniformBlocks(result);
      break;
    case kES3TransformFeedbackVaryings:
      {
        base::AutoUnlock unlock(lock_);
        // lock_ can't be held across IPC call or else it may deadlock in
        // pepper. http://crbug.com/418651
        gl->GetTransformFeedbackVaryingsCHROMIUMHelper(program, &result);
      }
      info->UpdateES3TransformFeedbackVaryings(result);
      break;
    case kES3Uniformsiv:
      {
        base::AutoUnlock unlock(lock_);
        // lock_ can't be held across IPC call or else it may deadlock in
        // pepper. http://crbug.com/418651
        gl->GetUniformsES3CHROMIUMHelper(program, &result);
      }
      info->UpdateES3Uniformsiv(result);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
  return info;
}

void ProgramInfoManager::CreateInfo(GLuint program) {
  base::AutoLock auto_lock(lock_);
  program_infos_.erase(program);
  std::pair<ProgramInfoMap::iterator, bool> result =
      program_infos_.insert(std::make_pair(program, Program()));

  DCHECK(result.second);
}

void ProgramInfoManager::DeleteInfo(GLuint program) {
  base::AutoLock auto_lock(lock_);
  program_infos_.erase(program);
}

bool ProgramInfoManager::GetProgramiv(
    GLES2Implementation* gl, GLuint program, GLenum pname, GLint* params) {
  base::AutoLock auto_lock(lock_);
  ProgramInfoType type = kNone;
  switch (pname) {
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
    case GL_ACTIVE_UNIFORMS:
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
    case GL_LINK_STATUS:
      type = kES2;
      break;
    case GL_ACTIVE_UNIFORM_BLOCKS:
    case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
      type = kES3UniformBlocks;
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_MODE:
    case GL_TRANSFORM_FEEDBACK_VARYINGS:
    case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH:
      type = kES3TransformFeedbackVaryings;
      break;
    default:
      return false;
  }
  Program* info = GetProgramInfo(gl, program, type);
  if (!info) {
    return false;
  }
  return info->GetProgramiv(pname, params);
}

bool ProgramInfoManager::GetActiveUniformsiv(
    GLES2Implementation* gl, GLuint program, GLsizei count,
    const GLuint* indices, GLenum pname, GLint* params) {
  base::AutoLock auto_lock(lock_);
  ProgramInfoType type = kNone;
  switch (pname) {
    case GL_UNIFORM_SIZE:
    case GL_UNIFORM_TYPE:
    case GL_UNIFORM_NAME_LENGTH:
      type = kES2;
      break;
    case GL_UNIFORM_BLOCK_INDEX:
    case GL_UNIFORM_OFFSET:
    case GL_UNIFORM_ARRAY_STRIDE:
    case GL_UNIFORM_MATRIX_STRIDE:
    case GL_UNIFORM_IS_ROW_MAJOR:
      type = kES3Uniformsiv;
      break;
    default:
      break;
  }
  if (type != kNone) {
    Program* info = GetProgramInfo(gl, program, type);
    if (info) {
      return info->GetUniformsiv(count, indices, pname, params);
    }
  }
  return gl->GetActiveUniformsivHelper(program, count, indices, pname, params);
}

GLint ProgramInfoManager::GetAttribLocation(
    GLES2Implementation* gl, GLuint program, const char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES2);
    if (info) {
      return info->GetAttribLocation(name);
    }
  }
  return gl->GetAttribLocationHelper(program, name);
}

GLint ProgramInfoManager::GetUniformLocation(
    GLES2Implementation* gl, GLuint program, const char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES2);
    if (info) {
      return info->GetUniformLocation(name);
    }
  }
  return gl->GetUniformLocationHelper(program, name);
}

GLint ProgramInfoManager::GetFragDataIndex(GLES2Implementation* gl,
                                           GLuint program,
                                           const char* name) {
  // TODO(zmo): make FragData indexes part of the ProgramInfo that are
  // fetched from the service side.  See crbug.com/452104.
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kNone);
    if (info) {
      GLint possible_index = info->GetFragDataIndex(name);
      if (possible_index != -1)
        return possible_index;
    }
  }
  GLint index = gl->GetFragDataIndexEXTHelper(program, name);
  if (index != -1) {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kNone);
    if (info) {
      info->CacheFragDataIndex(name, index);
    }
  }
  return index;
}

GLint ProgramInfoManager::GetFragDataLocation(
    GLES2Implementation* gl, GLuint program, const char* name) {
  // TODO(zmo): make FragData locations part of the ProgramInfo that are
  // fetched altogether from the service side.  See crbug.com/452104.
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kNone);
    if (info) {
      GLint possible_loc = info->GetFragDataLocation(name);
      if (possible_loc != -1)
        return possible_loc;
    }
  }
  GLint loc = gl->GetFragDataLocationHelper(program, name);
  if (loc != -1) {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kNone);
    if (info) {
      info->CacheFragDataLocation(name, loc);
    }
  }
  return loc;
}

bool ProgramInfoManager::GetActiveAttrib(
    GLES2Implementation* gl,
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
    GLint* size, GLenum* type, char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES2);
    if (info) {
      const Program::VertexAttrib* attrib_info = info->GetAttribInfo(index);
      if (attrib_info) {
        if (size) {
          *size = attrib_info->size;
        }
        if (type) {
          *type = attrib_info->type;
        }
        FillNameAndLength(bufsize, length, name, attrib_info->name);
        return true;
      }
    }
  }
  return gl->GetActiveAttribHelper(
      program, index, bufsize, length, size, type, name);
}

bool ProgramInfoManager::GetActiveUniform(
    GLES2Implementation* gl,
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
    GLint* size, GLenum* type, char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES2);
    if (info) {
      const Program::UniformInfo* uniform_info = info->GetUniformInfo(index);
      if (uniform_info) {
        if (size) {
          *size = uniform_info->size;
        }
        if (type) {
          *type = uniform_info->type;
        }
        FillNameAndLength(bufsize, length, name, uniform_info->name);
        return true;
      }
    }
  }
  return gl->GetActiveUniformHelper(
      program, index, bufsize, length, size, type, name);
}

GLuint ProgramInfoManager::GetUniformBlockIndex(
    GLES2Implementation* gl, GLuint program, const char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES3UniformBlocks);
    if (info) {
      return info->GetUniformBlockIndex(name);
    }
  }
  return gl->GetUniformBlockIndexHelper(program, name);
}

bool ProgramInfoManager::GetActiveUniformBlockName(
    GLES2Implementation* gl, GLuint program, GLuint index,
    GLsizei buf_size, GLsizei* length, char* name) {
  DCHECK_LE(0, buf_size);
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES3UniformBlocks);
    if (info) {
      const Program::UniformBlock* uniform_block = info->GetUniformBlock(index);
      if (uniform_block) {
        FillNameAndLength(buf_size, length, name, uniform_block->name);
        return true;
      }
    }
  }
  return gl->GetActiveUniformBlockNameHelper(
      program, index, buf_size, length, name);
}

bool ProgramInfoManager::GetActiveUniformBlockiv(
    GLES2Implementation* gl, GLuint program, GLuint index,
    GLenum pname, GLint* params) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES3UniformBlocks);
    if (info) {
      const Program::UniformBlock* uniform_block = info->GetUniformBlock(index);
      bool valid_pname;
      switch (pname) {
        case GL_UNIFORM_BLOCK_BINDING:
        case GL_UNIFORM_BLOCK_DATA_SIZE:
        case GL_UNIFORM_BLOCK_NAME_LENGTH:
        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
        case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
          valid_pname = true;
          break;
        default:
          valid_pname = false;
          break;
      }
      if (uniform_block && valid_pname && params) {
        switch (pname) {
          case GL_UNIFORM_BLOCK_BINDING:
            *params = static_cast<GLint>(uniform_block->binding);
            break;
          case GL_UNIFORM_BLOCK_DATA_SIZE:
            *params = static_cast<GLint>(uniform_block->data_size);
            break;
          case GL_UNIFORM_BLOCK_NAME_LENGTH:
            *params = static_cast<GLint>(uniform_block->name.size()) + 1;
            break;
          case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
            *params = static_cast<GLint>(
                uniform_block->active_uniform_indices.size());
            break;
          case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
            for (size_t ii = 0;
                 ii < uniform_block->active_uniform_indices.size(); ++ii) {
              params[ii] = static_cast<GLint>(
                  uniform_block->active_uniform_indices[ii]);
            }
            break;
          case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
            *params = static_cast<GLint>(
                uniform_block->referenced_by_vertex_shader);
            break;
          case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
            *params = static_cast<GLint>(
                uniform_block->referenced_by_fragment_shader);
            break;
          default:
            NOTREACHED_IN_MIGRATION();
        }
        return true;
      }
    }
  }
  return gl->GetActiveUniformBlockivHelper(program, index, pname, params);
}

void ProgramInfoManager::UniformBlockBinding(
    GLES2Implementation* gl, GLuint program, GLuint index, GLuint binding) {
  GLuint max_bindings =
      static_cast<GLuint>(gl->gl_capabilities().max_uniform_buffer_bindings);
  if (binding < max_bindings) {
    base::AutoLock auto_lock(lock_);
    // If UniformBlock info haven't been cached yet, skip updating the binding.
    Program* info = GetProgramInfo(gl, program, kNone);
    if (info) {
      info->UniformBlockBinding(index, binding);
    }
  }
}

bool ProgramInfoManager::GetTransformFeedbackVarying(
    GLES2Implementation* gl, GLuint program, GLuint index, GLsizei bufsize,
    GLsizei* length, GLsizei* size, GLenum* type, char* name) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES3TransformFeedbackVaryings);
    if (info) {
      const Program::TransformFeedbackVarying* varying =
          info->GetTransformFeedbackVarying(index);
      if (varying) {
        if (size) {
          *size = varying->size;
        }
        if (type) {
          *type = varying->type;
        }
        FillNameAndLength(bufsize, length, name, varying->name);
        return true;
      }
    }
  }
  return gl->GetTransformFeedbackVaryingHelper(
      program, index, bufsize, length, size, type, name);
}

bool ProgramInfoManager::GetUniformIndices(GLES2Implementation* gl,
    GLuint program, GLsizei count, const char* const* names, GLuint* indices) {
  {
    base::AutoLock auto_lock(lock_);
    Program* info = GetProgramInfo(gl, program, kES2);
    if (info) {
      DCHECK_LT(0, count);
      DCHECK(names && indices);
      for (GLsizei ii = 0; ii < count; ++ii) {
        indices[ii] = info->GetUniformIndex(names[ii]);
      }
      return true;
    }
  }
  return gl->GetUniformIndicesHelper(program, count, names, indices);
}

bool ProgramInfoManager::GetProgramInterfaceiv(
    GLES2Implementation* gl, GLuint program, GLenum program_interface,
    GLenum pname, GLint* params) {
  // TODO(jiajie.hu@intel.com): The info is not cached for now, so always
  // fallback to the IPC path.
  return false;
}

GLuint ProgramInfoManager::GetProgramResourceIndex(
    GLES2Implementation* gl, GLuint program, GLenum program_interface,
    const char* name) {
  // TODO(jiajie.hu@intel.com): The info is not cached for now, so always
  // fallback to the IPC path.
  return gl->GetProgramResourceIndexHelper(program, program_interface, name);
}

bool ProgramInfoManager::GetProgramResourceName(
    GLES2Implementation* gl, GLuint program, GLenum program_interface,
    GLuint index, GLsizei bufsize, GLsizei* length, char* name) {
  // TODO(jiajie.hu@intel.com): The info is not cached for now, so always
  // fallback to the IPC path.
  return gl->GetProgramResourceNameHelper(
      program, program_interface, index, bufsize, length, name);
}

bool ProgramInfoManager::GetProgramResourceiv(
    GLES2Implementation* gl, GLuint program, GLenum program_interface,
    GLuint index, GLsizei prop_count, const GLenum* props, GLsizei bufsize,
    GLsizei* length, GLint* params) {
  // TODO(jiajie.hu@intel.com): The info is not cached for now, so always
  // fallback to the IPC path.
  return gl->GetProgramResourceivHelper(
      program, program_interface, index, prop_count, props, bufsize, length,
      params);
}

GLint ProgramInfoManager::GetProgramResourceLocation(
    GLES2Implementation* gl, GLuint program, GLenum program_interface,
    const char* name) {
  // TODO(jiajie.hu@intel.com): The info is not cached for now, so always
  // fallback to the IPC path.
  return gl->GetProgramResourceLocationHelper(program, program_interface, name);
}

void ProgramInfoManager::UpdateProgramInfo(GLuint program,
                                           base::span<const int8_t> data,
                                           ProgramInfoType type) {
  base::AutoLock auto_lock(lock_);
  ProgramInfoMap::iterator it = program_infos_.find(program);
  // It's possible that the program has been deleted already. Imagine the code
  // snippet below:
  //   ...
  //   gl.linkProgram(program);
  //   gl.deleteProgram(program);
  //   ...
  if (it == program_infos_.end())
    return;
  Program* info = &it->second;
  switch (type) {
    case kES2:
      info->UpdateES2(data);
      break;
    case kES3UniformBlocks:
      info->UpdateES3UniformBlocks(data);
      break;
    case kES3TransformFeedbackVaryings:
      info->UpdateES3TransformFeedbackVaryings(data);
      break;
    case kES3Uniformsiv:
      info->UpdateES3Uniformsiv(data);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace gles2
}  // namespace gpu
