// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/service/multi_draw_manager.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace gpu {
namespace gles2 {

// Custom Handlers
error::Error GLES2DecoderPassthroughImpl::HandleBindAttribLocationBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindAttribLocationBucket& c =
      *static_cast<const volatile gles2::cmds::BindAttribLocationBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindAttribLocation(program, index, name_str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleBufferData(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BufferData& c =
      *static_cast<const volatile gles2::cmds::BufferData*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;
  GLenum usage = static_cast<GLenum>(c.usage);

  const void* data = nullptr;
  if (data_shm_id != 0 || data_shm_offset != 0) {
    data = GetSharedMemoryAs<const void*>(data_shm_id, data_shm_offset, size);
    if (!data) {
      return error::kOutOfBounds;
    }
  }
  return DoBufferData(target, size, data, usage);
}

error::Error GLES2DecoderPassthroughImpl::HandleClientWaitSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::ClientWaitSync& c =
      *static_cast<const volatile gles2::cmds::ClientWaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  const GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::ClientWaitSync::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      result_shm_id, result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  return DoClientWaitSync(sync, flags, timeout, result_dst);
}

error::Error GLES2DecoderPassthroughImpl::HandleCreateProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateProgram& c =
      *static_cast<const volatile gles2::cmds::CreateProgram*>(cmd_data);
  GLuint client_id = static_cast<GLuint>(c.client_id);

  return DoCreateProgram(client_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleCreateShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateShader& c =
      *static_cast<const volatile gles2::cmds::CreateShader*>(cmd_data);
  GLenum type = static_cast<GLenum>(c.type);
  GLuint client_id = static_cast<GLuint>(c.client_id);

  return DoCreateShader(type, client_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleFenceSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::FenceSync& c =
      *static_cast<const volatile gles2::cmds::FenceSync*>(cmd_data);
  GLenum condition = static_cast<GLenum>(c.condition);
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  GLuint client_id = static_cast<GLuint>(c.client_id);

  return DoFenceSync(condition, flags, client_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawArrays(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawArrays& c =
      *static_cast<const volatile gles2::cmds::DrawArrays*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);

  return DoDrawArrays(mode, first, count);
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawArraysIndirect(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawArraysIndirect& c =
      *static_cast<const volatile gles2::cmds::DrawArraysIndirect*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  const void* offset =
      reinterpret_cast<const void*>(static_cast<uintptr_t>(c.offset));

  return DoDrawArraysIndirect(mode, offset);
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawElements(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElements& c =
      *static_cast<const volatile gles2::cmds::DrawElements*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  const GLvoid* indices =
      reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(c.index_offset));

  return DoDrawElements(mode, count, type, indices);
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawElementsIndirect(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawElementsIndirect& c =
      *static_cast<const volatile gles2::cmds::DrawElementsIndirect*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  const void* offset =
      reinterpret_cast<const void*>(static_cast<uintptr_t>(c.offset));

  return DoDrawElementsIndirect(mode, type, offset);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveAttrib(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetActiveAttrib& c =
      *static_cast<const volatile gles2::cmds::GetActiveAttrib*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetActiveAttrib::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  std::string name;
  error::Error error = DoGetActiveAttrib(
      program, index, &result->size, &result->type, &name, &result->success);
  if (error != error::kNoError) {
    result->success = 0;
    return error;
  }

  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniform(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetActiveUniform& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniform*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetActiveUniform::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  std::string name;
  error::Error error = DoGetActiveUniform(
      program, index, &result->size, &result->type, &name, &result->success);
  if (error != error::kNoError) {
    result->success = 0;
    return error;
  }

  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniformBlockiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetActiveUniformBlockiv& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformBlockiv*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint uniformBlockIndex = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;

  unsigned int buffer_size = 0;
  typedef cmds::GetActiveUniformBlockiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      params_shm_id, params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error = DoGetActiveUniformBlockiv(
      program, uniformBlockIndex, pname, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniformBlockName(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetActiveUniformBlockName& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformBlockName*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint uniformBlockIndex = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetActiveUniformBlockName::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }

  std::string name;
  error::Error error =
      DoGetActiveUniformBlockName(program, uniformBlockIndex, &name);
  if (error != error::kNoError) {
    return error;
  }

  *result = 1;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetActiveUniformsiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetActiveUniformsiv& c =
      *static_cast<const volatile gles2::cmds::GetActiveUniformsiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;
  uint32_t indices_bucket_id = c.indices_bucket_id;

  Bucket* bucket = GetBucket(indices_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei uniformCount = static_cast<GLsizei>(bucket->size() / sizeof(GLuint));
  const GLuint* indices = bucket->GetDataAs<const GLuint*>(0, bucket->size());
  typedef cmds::GetActiveUniformsiv::Result Result;
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(uniformCount).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(params_shm_id, params_shm_offset,
                                              checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }

  error::Error error =
      DoGetActiveUniformsiv(program, uniformCount, indices, pname, params);
  if (error != error::kNoError) {
    return error;
  }

  result->SetNumResults(uniformCount);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetAttachedShaders(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetAttachedShaders& c =
      *static_cast<const volatile gles2::cmds::GetAttachedShaders*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t result_size = c.result_size;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetAttachedShaders::Result Result;
  uint32_t maxCount = Result::ComputeMaxResults(result_size);
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(maxCount).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              checked_size);
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  error::Error error =
      DoGetAttachedShaders(program, maxCount, &count, result->GetData());
  if (error != error::kNoError) {
    return error;
  }

  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetAttribLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetAttribLocation& c =
      *static_cast<const volatile gles2::cmds::GetAttribLocation*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t location_shm_id = c.location_shm_id;
  uint32_t location_shm_offset = c.location_shm_offset;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  return DoGetAttribLocation(program, name_str.c_str(), location);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetFragDataLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetFragDataLocation& c =
      *static_cast<const volatile gles2::cmds::GetFragDataLocation*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t location_shm_id = c.location_shm_id;
  uint32_t location_shm_offset = c.location_shm_offset;
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  return DoGetFragDataLocation(program, name_str.c_str(), location);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetInternalformativ(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetInternalformativ& c =
      *static_cast<const volatile gles2::cmds::GetInternalformativ*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.format);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;

  unsigned int buffer_size = 0;
  typedef cmds::GetInternalformativ::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      params_shm_id, params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error = DoGetInternalformativ(target, internalformat, pname,
                                             bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramInfoLog(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramInfoLog& c =
      *static_cast<const volatile gles2::cmds::GetProgramInfoLog*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;

  std::string infolog;
  error::Error error = DoGetProgramInfoLog(program, &infolog);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(infolog.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramResourceiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2ComputeContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetProgramResourceiv& c =
      *static_cast<const volatile gles2::cmds::GetProgramResourceiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLenum program_interface = static_cast<GLenum>(c.program_interface);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t props_bucket_id = c.props_bucket_id;
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;

  Bucket* bucket = GetBucket(props_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei prop_count = static_cast<GLsizei>(bucket->size() / sizeof(GLenum));
  const GLenum* props = bucket->GetDataAs<const GLenum*>(0, bucket->size());
  unsigned int buffer_size = 0;
  typedef cmds::GetProgramResourceiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      params_shm_id, params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error = DoGetProgramResourceiv(
      program, program_interface, index, prop_count, props, bufsize, &length,
      params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramResourceIndex(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2ComputeContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetProgramResourceIndex& c =
      *static_cast<const volatile gles2::cmds::GetProgramResourceIndex*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLenum program_interface = static_cast<GLenum>(c.program_interface);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t index_shm_id = c.index_shm_id;
  uint32_t index_shm_offset = c.index_shm_offset;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLuint* index = GetSharedMemoryAs<GLuint*>(
      index_shm_id, index_shm_offset, sizeof(GLuint));
  if (!index) {
    return error::kOutOfBounds;
  }
  if (*index != GL_INVALID_INDEX) {
    return error::kInvalidArguments;
  }
  return DoGetProgramResourceIndex(
      program, program_interface, name_str.c_str(), index);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramResourceLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2ComputeContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetProgramResourceLocation& c =
      *static_cast<const volatile gles2::cmds::GetProgramResourceLocation*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLenum program_interface = static_cast<GLenum>(c.program_interface);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t location_shm_id = c.location_shm_id;
  uint32_t location_shm_offset = c.location_shm_offset;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  return DoGetProgramResourceLocation(
      program, program_interface, name_str.c_str(), location);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramResourceName(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2ComputeContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetProgramResourceName& c =
      *static_cast<const volatile gles2::cmds::GetProgramResourceName*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLenum program_interface = static_cast<GLenum>(c.program_interface);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetProgramResourceName::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(
      result_shm_id, result_shm_offset, sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  std::string name;
  error::Error error =
      DoGetProgramResourceName(program, program_interface, index, &name);
  if (error != error::kNoError) {
    return error;
  }
  *result = 1;
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderInfoLog(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderInfoLog& c =
      *static_cast<const volatile gles2::cmds::GetShaderInfoLog*>(cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);
  uint32_t bucket_id = c.bucket_id;

  std::string infolog;
  error::Error error = DoGetShaderInfoLog(shader, &infolog);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(infolog.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderPrecisionFormat(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderPrecisionFormat& c =
      *static_cast<const volatile gles2::cmds::GetShaderPrecisionFormat*>(
          cmd_data);
  GLenum shader_type = static_cast<GLenum>(c.shadertype);
  GLenum precision_type = static_cast<GLenum>(c.precisiontype);
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetShaderPrecisionFormat::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  GLint range[2] = {0, 0};
  GLint precision = 0;
  error::Error error = DoGetShaderPrecisionFormat(
      shader_type, precision_type, range, &precision, &result->success);
  if (error != error::kNoError) {
    result->success = 0;
    return error;
  }

  result->min_range = range[0];
  result->max_range = range[1];
  result->precision = precision;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetShaderSource(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderSource& c =
      *static_cast<const volatile gles2::cmds::GetShaderSource*>(cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);
  uint32_t bucket_id = c.bucket_id;

  std::string source;
  error::Error error = DoGetShaderSource(shader, &source);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(source.c_str());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetString(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetString& c =
      *static_cast<const volatile gles2::cmds::GetString*>(cmd_data);
  GLenum name = static_cast<GLenum>(c.name);
  uint32_t bucket_id = c.bucket_id;

  return DoGetString(name, bucket_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetTransformFeedbackVarying(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetTransformFeedbackVarying& c =
      *static_cast<const volatile gles2::cmds::GetTransformFeedbackVarying*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  typedef cmds::GetTransformFeedbackVarying::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->success != 0) {
    return error::kInvalidArguments;
  }

  GLsizei size = 0;
  GLenum type = 0;
  std::string name;
  error::Error error = DoGetTransformFeedbackVarying(
      program, index, &size, &type, &name, &result->success);
  if (error != error::kNoError) {
    result->success = 0;
    return error;
  }

  result->size = static_cast<int32_t>(size);
  result->type = static_cast<uint32_t>(type);
  Bucket* bucket = CreateBucket(name_bucket_id);
  bucket->SetFromString(name.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformBlockIndex(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetUniformBlockIndex& c =
      *static_cast<const volatile gles2::cmds::GetUniformBlockIndex*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t index_shm_id = c.index_shm_id;
  uint32_t index_shm_offset = c.index_shm_offset;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* index =
      GetSharedMemoryAs<GLint*>(index_shm_id, index_shm_offset, sizeof(GLint));
  if (!index) {
    return error::kOutOfBounds;
  }
  if (*index != -1) {
    return error::kInvalidArguments;
  }
  return DoGetUniformBlockIndex(program, name_str.c_str(), index);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformfv& c =
      *static_cast<const volatile gles2::cmds::GetUniformfv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;

  unsigned int buffer_size = 0;
  typedef cmds::GetUniformfv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      params_shm_id, params_shm_offset, sizeof(Result), &buffer_size);
  GLfloat* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetUniformfv(program, location, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformiv& c =
      *static_cast<const volatile gles2::cmds::GetUniformiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;

  unsigned int buffer_size = 0;
  typedef cmds::GetUniformiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      params_shm_id, params_shm_offset, sizeof(Result), &buffer_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetUniformiv(program, location, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformuiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetUniformuiv& c =
      *static_cast<const volatile gles2::cmds::GetUniformuiv*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32_t params_shm_id = c.params_shm_id;
  uint32_t params_shm_offset = c.params_shm_offset;

  unsigned int buffer_size = 0;
  typedef cmds::GetUniformuiv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      params_shm_id, params_shm_offset, sizeof(Result), &buffer_size);
  GLuint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetUniformuiv(program, location, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformIndices(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetUniformIndices& c =
      *static_cast<const volatile gles2::cmds::GetUniformIndices*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t names_bucket_id = c.names_bucket_id;
  uint32_t indices_shm_id = c.indices_shm_id;
  uint32_t indices_shm_offset = c.indices_shm_offset;

  Bucket* bucket = GetBucket(names_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> names;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &names, &len) || count <= 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::GetUniformIndices::Result Result;
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(count).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(indices_shm_id,
                                              indices_shm_offset, checked_size);
  GLuint* indices = result ? result->GetData() : nullptr;
  if (indices == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  error::Error error =
      DoGetUniformIndices(program, count, &names[0], count, indices);
  if (error != error::kNoError) {
    return error;
  }
  result->SetNumResults(count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetUniformLocation& c =
      *static_cast<const volatile gles2::cmds::GetUniformLocation*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t location_shm_id = c.location_shm_id;
  uint32_t location_shm_offset = c.location_shm_offset;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* location = GetSharedMemoryAs<GLint*>(
      location_shm_id, location_shm_offset, sizeof(GLint));
  if (!location) {
    return error::kOutOfBounds;
  }
  if (*location != -1) {
    return error::kInvalidArguments;
  }
  return DoGetUniformLocation(program, name_str.c_str(), location);
}

error::Error GLES2DecoderPassthroughImpl::HandleGetVertexAttribPointerv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribPointerv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribPointerv*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t pointer_shm_id = c.pointer_shm_id;
  uint32_t pointer_shm_offset = c.pointer_shm_offset;

  unsigned int buffer_size = 0;
  typedef cmds::GetVertexAttribPointerv::Result Result;
  Result* result = GetSharedMemoryAndSizeAs<Result*>(
      pointer_shm_id, pointer_shm_offset, sizeof(Result), &buffer_size);
  GLuint* params = result ? result->GetData() : nullptr;
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  GLsizei bufsize = Result::ComputeMaxResults(buffer_size);
  GLsizei length = 0;
  error::Error error =
      DoGetVertexAttribPointerv(index, pname, bufsize, &length, params);
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }
  result->SetNumResults(length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePixelStorei(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PixelStorei& c =
      *static_cast<const volatile gles2::cmds::PixelStorei*>(cmd_data);
  GLenum pname = static_cast<GLuint>(c.pname);
  GLint param = static_cast<GLint>(c.param);

  return DoPixelStorei(pname, param);
}

error::Error GLES2DecoderPassthroughImpl::HandleReadPixels(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ReadPixels& c =
      *static_cast<const volatile gles2::cmds::ReadPixels*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;
  GLboolean async = static_cast<GLboolean>(c.async);

  bool pack_buffer_bound = bound_buffers_[GL_PIXEL_PACK_BUFFER] != 0;

  uint8_t* pixels = nullptr;
  unsigned int buffer_size = 0;
  if (pixels_shm_id != 0) {
    if (pack_buffer_bound) {
      return error::kInvalidArguments;
    }
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, 0, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  } else {
    if (!pack_buffer_bound) {
      return error::kInvalidArguments;
    }
    pixels =
        reinterpret_cast<uint8_t*>(static_cast<intptr_t>(pixels_shm_offset));
  }

  GLsizei bufsize = buffer_size;
  GLsizei length = 0;
  GLsizei columns = 0;
  GLsizei rows = 0;
  int32_t success = 0;
  error::Error error = error::kNoError;
  if (async && feature_info_->feature_flags().use_async_readpixels &&
      !pack_buffer_bound) {
    error = DoReadPixelsAsync(
        x, y, width, height, format, type, bufsize, &length, &columns, &rows,
        pixels_shm_id, pixels_shm_offset, result_shm_id, result_shm_offset);
  } else {
    error = DoReadPixels(x, y, width, height, format, type, bufsize, &length,
                         &columns, &rows, pixels, &success);
  }
  if (error != error::kNoError) {
    return error;
  }
  if (length > bufsize) {
    return error::kOutOfBounds;
  }

  if (result_shm_id != 0) {
    typedef cmds::ReadPixels::Result Result;
    Result* result = GetSharedMemoryAs<Result*>(
        result_shm_id, result_shm_offset, sizeof(*result));
    if (!result) {
      return error::kOutOfBounds;
    }
    if (result->success != 0) {
      return error::kInvalidArguments;
    }

    result->success = success;
    result->row_length = static_cast<uint32_t>(columns);
    result->num_rows = static_cast<uint32_t>(rows);
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleShaderBinary(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ShaderBinary& c =
      *static_cast<const volatile gles2::cmds::ShaderBinary*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  GLsizei length = static_cast<GLsizei>(c.length);
  GLenum binaryformat = static_cast<GLenum>(c.binaryformat);
  uint32_t shaders_shm_id = c.shaders_shm_id;
  uint32_t shaders_shm_offset = c.shaders_shm_offset;
  uint32_t binary_shm_id = c.binary_shm_id;
  uint32_t binary_shm_offset = c.binary_shm_offset;

  uint32_t data_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&data_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* shaders = GetSharedMemoryAs<const GLuint*>(
      shaders_shm_id, shaders_shm_offset, data_size);
  const void* binary =
      GetSharedMemoryAs<const void*>(binary_shm_id, binary_shm_offset, length);
  if (shaders == nullptr || binary == nullptr) {
    return error::kOutOfBounds;
  }

  return DoShaderBinary(n, shaders, binaryformat, binary, length);
}

error::Error GLES2DecoderPassthroughImpl::HandleTexImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexImage2D& c =
      *static_cast<const volatile gles2::cmds::TexImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;

  unsigned int buffer_size = 0;
  const void* pixels = nullptr;

  if (pixels_shm_id != 0) {
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, 0, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  } else {
    pixels =
        reinterpret_cast<const void*>(static_cast<intptr_t>(pixels_shm_offset));
  }

  return DoTexImage2D(target, level, internal_format, width, height, border,
                      format, type, buffer_size, pixels);
}

error::Error GLES2DecoderPassthroughImpl::HandleTexImage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::TexImage3D& c =
      *static_cast<const volatile gles2::cmds::TexImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint internal_format = static_cast<GLint>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;

  unsigned int buffer_size = 0;
  const void* pixels = nullptr;

  if (pixels_shm_id != 0) {
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, 0, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  } else {
    pixels =
        reinterpret_cast<const void*>(static_cast<intptr_t>(pixels_shm_offset));
  }

  return DoTexImage3D(target, level, internal_format, width, height, depth,
                      border, format, type, buffer_size, pixels);
}

error::Error GLES2DecoderPassthroughImpl::HandleTexSubImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::TexSubImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;

  unsigned int buffer_size = 0;
  const void* pixels = nullptr;

  if (pixels_shm_id != 0) {
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, 0, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  } else {
    pixels =
        reinterpret_cast<const void*>(static_cast<intptr_t>(pixels_shm_offset));
  }

  return DoTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                         type, buffer_size, pixels);
}

error::Error GLES2DecoderPassthroughImpl::HandleTexSubImage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::TexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::TexSubImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLenum type = static_cast<GLenum>(c.type);
  uint32_t pixels_shm_id = c.pixels_shm_id;
  uint32_t pixels_shm_offset = c.pixels_shm_offset;

  unsigned int buffer_size = 0;
  const void* pixels = nullptr;

  if (pixels_shm_id != 0) {
    pixels = GetSharedMemoryAndSizeAs<uint8_t*>(
        pixels_shm_id, pixels_shm_offset, 0, &buffer_size);
    if (!pixels) {
      return error::kOutOfBounds;
    }
  } else {
    pixels =
        reinterpret_cast<const void*>(static_cast<intptr_t>(pixels_shm_offset));
  }

  return DoTexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                         height, depth, format, type, buffer_size, pixels);
}

error::Error GLES2DecoderPassthroughImpl::HandleUniformBlockBinding(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::UniformBlockBinding& c =
      *static_cast<const volatile gles2::cmds::UniformBlockBinding*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint binding = static_cast<GLuint>(c.binding);

  return DoUniformBlockBinding(program, index, binding);
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribIPointer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::VertexAttribIPointer& c =
      *static_cast<const volatile gles2::cmds::VertexAttribIPointer*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.indx);
  GLint size = static_cast<GLint>(c.size);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei stride = static_cast<GLsizei>(c.stride);
  GLsizei offset = static_cast<GLsizei>(c.offset);
  const void* ptr = reinterpret_cast<const void*>(offset);

  return DoVertexAttribIPointer(index, size, type, stride, ptr);
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribPointer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttribPointer& c =
      *static_cast<const volatile gles2::cmds::VertexAttribPointer*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.indx);
  GLint size = static_cast<GLint>(c.size);
  GLenum type = static_cast<GLenum>(c.type);
  GLboolean normalized = static_cast<GLenum>(c.normalized);
  GLsizei stride = static_cast<GLsizei>(c.stride);
  GLsizei offset = static_cast<GLsizei>(c.offset);
  const void* ptr = reinterpret_cast<const void*>(offset);

  return DoVertexAttribPointer(index, size, type, normalized, stride, ptr);
}

error::Error GLES2DecoderPassthroughImpl::HandleWaitSync(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::WaitSync& c =
      *static_cast<const volatile gles2::cmds::WaitSync*>(cmd_data);
  const GLuint sync = static_cast<GLuint>(c.sync);
  const GLbitfield flags = static_cast<GLbitfield>(c.flags);
  const GLuint64 timeout = c.timeout();

  return DoWaitSync(sync, flags, timeout);
}

error::Error GLES2DecoderPassthroughImpl::HandleQueryCounterEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_disjoint_timer_query) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::QueryCounterEXT& c =
      *static_cast<const volatile gles2::cmds::QueryCounterEXT*>(cmd_data);
  GLuint id = static_cast<GLuint>(c.id);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t sync_shm_id = c.sync_data_shm_id;
  uint32_t sync_shm_offset = c.sync_data_shm_offset;
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  return DoQueryCounterEXT(id, target, sync_shm_id, sync_shm_offset,
                           submit_count);
}

error::Error GLES2DecoderPassthroughImpl::HandleBeginQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BeginQueryEXT& c =
      *static_cast<const volatile gles2::cmds::BeginQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint id = static_cast<GLuint>(c.id);
  uint32_t sync_shm_id = c.sync_data_shm_id;
  uint32_t sync_shm_offset = c.sync_data_shm_offset;

  return DoBeginQueryEXT(target, id, sync_shm_id, sync_shm_offset);
}

error::Error GLES2DecoderPassthroughImpl::HandleEndQueryEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EndQueryEXT& c =
      *static_cast<const volatile gles2::cmds::EndQueryEXT*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  uint32_t submit_count = static_cast<GLuint>(c.submit_count);

  return DoEndQueryEXT(target, submit_count);
}

error::Error GLES2DecoderPassthroughImpl::HandleSetDisjointValueSyncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetDisjointValueSyncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetDisjointValueSyncCHROMIUM*>(
          cmd_data);
  uint32_t sync_data_shm_id = c.sync_data_shm_id;
  uint32_t sync_data_shm_offset = c.sync_data_shm_offset;

  DisjointValueSync* sync = GetSharedMemoryAs<DisjointValueSync*>(
      sync_data_shm_id, sync_data_shm_offset, sizeof(*sync));
  if (!sync) {
    return error::kOutOfBounds;
  }
  return DoSetDisjointValueSyncCHROMIUM(sync);
}

error::Error GLES2DecoderPassthroughImpl::HandleInsertEventMarkerEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_debug_marker) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::InsertEventMarkerEXT& c =
      *static_cast<const volatile gles2::cmds::InsertEventMarkerEXT*>(cmd_data);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  return DoInsertEventMarkerEXT(0, str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandlePushGroupMarkerEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_debug_marker) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::PushGroupMarkerEXT& c =
      *static_cast<const volatile gles2::cmds::PushGroupMarkerEXT*>(cmd_data);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  return DoPushGroupMarkerEXT(0, str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleEnableFeatureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EnableFeatureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::EnableFeatureCHROMIUM*>(
          cmd_data);
  uint32_t bucket_id = c.bucket_id;
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  typedef cmds::EnableFeatureCHROMIUM::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*result != 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }
  error::Error error = DoEnableFeatureCHROMIUM(feature_str.c_str());
  if (error != error::kNoError) {
    return error;
  }

  *result = 1;  // true.
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleMapBufferRange(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::MapBufferRange& c =
      *static_cast<const volatile gles2::cmds::MapBufferRange*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLbitfield access = static_cast<GLbitfield>(c.access);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t result_shm_id = c.result_shm_id;
  uint32_t result_shm_offset = c.result_shm_offset;
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  typedef cmds::MapBufferRange::Result Result;
  Result* result = GetSharedMemoryAs<Result*>(result_shm_id, result_shm_offset,
                                              sizeof(*result));
  if (!result) {
    return error::kOutOfBounds;
  }
  if (*result != 0) {
    *result = 0;
    return error::kInvalidArguments;
  }
  uint8_t* mem =
      GetSharedMemoryAs<uint8_t*>(data_shm_id, data_shm_offset, size);
  if (!mem) {
    return error::kOutOfBounds;
  }

  error::Error error = DoMapBufferRange(target, offset, size, access, mem,
                                        data_shm_id, data_shm_offset, result);
  DCHECK(error == error::kNoError || *result == 0);
  return error;
}

error::Error GLES2DecoderPassthroughImpl::HandleUnmapBuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::UnmapBuffer& c =
      *static_cast<const volatile gles2::cmds::UnmapBuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);

  return DoUnmapBuffer(target);
}

error::Error GLES2DecoderPassthroughImpl::HandleResizeCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ResizeCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ResizeCHROMIUM*>(cmd_data);
  GLuint width = static_cast<GLuint>(c.width);
  GLuint height = static_cast<GLuint>(c.height);
  GLfloat scale_factor = static_cast<GLfloat>(c.scale_factor);
  GLenum color_space = static_cast<GLenum>(c.color_space);
  GLboolean has_alpha = static_cast<GLboolean>(c.alpha);

  return DoResizeCHROMIUM(width, height, scale_factor, color_space, has_alpha);
}

error::Error
GLES2DecoderPassthroughImpl::HandleGetRequestableExtensionsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().angle_request_extension) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetRequestableExtensionsCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::GetRequestableExtensionsCHROMIUM*>(
          cmd_data);
  uint32_t bucket_id = c.bucket_id;

  const char* str = nullptr;
  error::Error error = DoGetRequestableExtensionsCHROMIUM(&str);
  if (error != error::kNoError) {
    return error;
  }
  if (!str) {
    return error::kOutOfBounds;
  }
  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(str);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleRequestExtensionCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().angle_request_extension) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::RequestExtensionCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::RequestExtensionCHROMIUM*>(
          cmd_data);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string feature_str;
  if (!bucket->GetAsString(&feature_str)) {
    return error::kInvalidArguments;
  }
  return DoRequestExtensionCHROMIUM(feature_str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleGetProgramInfoCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramInfoCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetProgramInfoCHROMIUM*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(ProgramInfoHeader));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetProgramInfoCHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformBlocksCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetUniformBlocksCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetUniformBlocksCHROMIUM*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformBlocksHeader));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetUniformBlocksCHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleGetTransformFeedbackVaryingsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(TransformFeedbackVaryingsHeader));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetTransformFeedbackVaryingsCHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetUniformsES3CHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetUniformsES3CHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetUniformsES3CHROMIUM*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetSize(sizeof(UniformsES3Header));  // in case we fail.

  std::vector<uint8_t> data;
  error::Error error = DoGetUniformsES3CHROMIUM(program, &data);
  if (error != error::kNoError) {
    return error;
  }

  bucket->SetSize(data.size());
  bucket->SetData(data.data(), 0, data.size());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleGetTranslatedShaderSourceANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().angle_translated_shader_source) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetTranslatedShaderSourceANGLE& c =
      *static_cast<const volatile gles2::cmds::GetTranslatedShaderSourceANGLE*>(
          cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);
  uint32_t bucket_id = c.bucket_id;

  std::string source;
  error::Error error = DoGetTranslatedShaderSourceANGLE(shader, &source);
  if (error != error::kNoError) {
    return error;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(source.c_str());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandlePostSubBufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PostSubBufferCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PostSubBufferCHROMIUM*>(
          cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint width = static_cast<GLint>(c.width);
  GLint height = static_cast<GLint>(c.height);
  GLuint64 swap_id = static_cast<GLuint64>(c.swap_id());
  GLbitfield flags = static_cast<GLbitfield>(c.flags);

  return DoPostSubBufferCHROMIUM(swap_id, x, y, width, height, flags);
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawArraysInstancedANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().angle_instanced_arrays) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::DrawArraysInstancedANGLE& c =
      *static_cast<const volatile gles2::cmds::DrawArraysInstancedANGLE*>(
          cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);

  return DoDrawArraysInstancedANGLE(mode, first, count, primcount);
}

error::Error
GLES2DecoderPassthroughImpl::HandleDrawArraysInstancedBaseInstanceANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().webgl_draw_instanced_base_vertex_base_instance) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::DrawArraysInstancedBaseInstanceANGLE& c =
      *static_cast<
          const volatile gles2::cmds::DrawArraysInstancedBaseInstanceANGLE*>(
          cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLint first = static_cast<GLint>(c.first);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  GLuint baseinstance = static_cast<GLsizei>(c.baseinstance);

  return DoDrawArraysInstancedBaseInstanceANGLE(mode, first, count, primcount,
                                                baseinstance);
}

error::Error GLES2DecoderPassthroughImpl::HandleDrawElementsInstancedANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().angle_instanced_arrays) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::DrawElementsInstancedANGLE& c =
      *static_cast<const volatile gles2::cmds::DrawElementsInstancedANGLE*>(
          cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  const GLvoid* indices =
      reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(c.index_offset));
  GLsizei primcount = static_cast<GLsizei>(c.primcount);

  return DoDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
}

error::Error GLES2DecoderPassthroughImpl::
    HandleDrawElementsInstancedBaseVertexBaseInstanceANGLE(
        uint32_t immediate_data_size,
        const volatile void* cmd_data) {
  if (!features().webgl_draw_instanced_base_vertex_base_instance) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE&
      c = *static_cast<const volatile gles2::cmds::
                           DrawElementsInstancedBaseVertexBaseInstanceANGLE*>(
          cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  const GLvoid* indices =
      reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(c.index_offset));
  GLsizei primcount = static_cast<GLsizei>(c.primcount);
  GLsizei basevertex = static_cast<GLsizei>(c.basevertex);
  GLsizei baseinstance = static_cast<GLsizei>(c.baseinstance);

  return DoDrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, count, type, indices, primcount, basevertex, baseinstance);
}

error::Error GLES2DecoderPassthroughImpl::HandleMultiDrawArraysCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawArraysCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MultiDrawArraysCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t firsts_size, counts_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&firsts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  const GLint* firsts = GetSharedMemoryAs<const GLint*>(
      c.firsts_shm_id, c.firsts_shm_offset, firsts_size);
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  if (firsts == nullptr) {
    return error::kOutOfBounds;
  }
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawArrays(mode, firsts, counts, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleMultiDrawArraysInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawArraysInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::MultiDrawArraysInstancedCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t firsts_size, counts_size, instance_counts_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&firsts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  const GLint* firsts = GetSharedMemoryAs<const GLint*>(
      c.firsts_shm_id, c.firsts_shm_offset, firsts_size);
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  if (firsts == nullptr) {
    return error::kOutOfBounds;
  }
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawArraysInstanced(
          mode, firsts, counts, instance_counts, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleMultiDrawArraysInstancedBaseInstanceCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       MultiDrawArraysInstancedBaseInstanceCHROMIUM*>(cmd_data);
  if (!features().webgl_multi_draw_instanced_base_vertex_base_instance) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t firsts_size, counts_size, instance_counts_size, baseinstances_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&firsts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLuint)).AssignIfValid(&baseinstances_size)) {
    return error::kOutOfBounds;
  }
  const GLint* firsts = GetSharedMemoryAs<const GLint*>(
      c.firsts_shm_id, c.firsts_shm_offset, firsts_size);
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  const GLuint* baseinstances = GetSharedMemoryAs<const GLuint*>(
      c.baseinstances_shm_id, c.baseinstances_shm_offset, baseinstances_size);
  if (firsts == nullptr) {
    return error::kOutOfBounds;
  }
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (baseinstances == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawArraysInstancedBaseInstance(
          mode, firsts, counts, instance_counts, baseinstances, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleMultiDrawElementsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawElementsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MultiDrawElementsCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t counts_size, offsets_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&offsets_size)) {
    return error::kOutOfBounds;
  }
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* offsets = GetSharedMemoryAs<const GLsizei*>(
      c.offsets_shm_id, c.offsets_shm_offset, offsets_size);
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (offsets == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawElements(mode, counts, type, offsets,
                                              drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::HandleMultiDrawElementsInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawElementsInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::MultiDrawElementsInstancedCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t counts_size, offsets_size, instance_counts_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&offsets_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* offsets = GetSharedMemoryAs<const GLsizei*>(
      c.offsets_shm_id, c.offsets_shm_offset, offsets_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (offsets == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawElementsInstanced(
          mode, counts, type, offsets, instance_counts, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::
    HandleMultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM(
        uint32_t immediate_data_size,
        const volatile void* cmd_data) {
  const volatile gles2::cmds::
      MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM& c =
          *static_cast<
              const volatile gles2::cmds::
                  MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM*>(
              cmd_data);
  if (!features().webgl_multi_draw_instanced_base_vertex_base_instance) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLenum type = static_cast<GLenum>(c.type);
  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);

  uint32_t counts_size, offsets_size, instance_counts_size, basevertices_size,
      baseinstances_size;
  base::CheckedNumeric<uint32_t> checked_size(drawcount);
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&offsets_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLsizei)).AssignIfValid(&instance_counts_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLint)).AssignIfValid(&basevertices_size)) {
    return error::kOutOfBounds;
  }
  if (!(checked_size * sizeof(GLuint)).AssignIfValid(&baseinstances_size)) {
    return error::kOutOfBounds;
  }
  const GLsizei* counts = GetSharedMemoryAs<const GLsizei*>(
      c.counts_shm_id, c.counts_shm_offset, counts_size);
  const GLsizei* offsets = GetSharedMemoryAs<const GLsizei*>(
      c.offsets_shm_id, c.offsets_shm_offset, offsets_size);
  const GLsizei* instance_counts = GetSharedMemoryAs<const GLsizei*>(
      c.instance_counts_shm_id, c.instance_counts_shm_offset,
      instance_counts_size);
  const GLint* basevertices = GetSharedMemoryAs<const GLint*>(
      c.basevertices_shm_id, c.basevertices_shm_offset, basevertices_size);
  const GLuint* baseinstances = GetSharedMemoryAs<const GLuint*>(
      c.baseinstances_shm_id, c.baseinstances_shm_offset, baseinstances_size);
  if (counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (offsets == nullptr) {
    return error::kOutOfBounds;
  }
  if (instance_counts == nullptr) {
    return error::kOutOfBounds;
  }
  if (!multi_draw_manager_->MultiDrawElementsInstancedBaseVertexBaseInstance(
          mode, counts, type, offsets, instance_counts, basevertices,
          baseinstances, drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::HandleVertexAttribDivisorANGLE(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().angle_instanced_arrays) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::VertexAttribDivisorANGLE& c =
      *static_cast<const volatile gles2::cmds::VertexAttribDivisorANGLE*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint divisor = static_cast<GLuint>(c.divisor);

  return DoVertexAttribDivisorANGLE(index, divisor);
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindUniformLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindUniformLocationCHROMIUMBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindUniformLocationCHROMIUMBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindUniformLocationCHROMIUM(program, location, name_str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleTraceBeginCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TraceBeginCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::TraceBeginCHROMIUM*>(cmd_data);
  uint32_t name_bucket_id = c.name_bucket_id;
  uint32_t category_bucket_id = c.category_bucket_id;

  Bucket* category_bucket = GetBucket(category_bucket_id);
  Bucket* name_bucket = GetBucket(name_bucket_id);
  static constexpr size_t kMaxStrLen = 256;
  if (!category_bucket || category_bucket->size() == 0 ||
      category_bucket->size() > kMaxStrLen || !name_bucket ||
      name_bucket->size() == 0 || name_bucket->size() > kMaxStrLen) {
    return error::kInvalidArguments;
  }

  std::string category_name;
  std::string trace_name;
  if (!category_bucket->GetAsString(&category_name) ||
      !name_bucket->GetAsString(&trace_name)) {
    return error::kInvalidArguments;
  }

  return DoTraceBeginCHROMIUM(category_name.c_str(), trace_name.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleDescheduleUntilFinishedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return DoDescheduleUntilFinishedCHROMIUM();
}

error::Error GLES2DecoderPassthroughImpl::HandleDiscardBackbufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return DoDiscardBackbufferCHROMIUM();
}

error::Error GLES2DecoderPassthroughImpl::HandleScheduleOverlayPlaneCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleOverlayPlaneCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ScheduleOverlayPlaneCHROMIUM*>(
          cmd_data);
  GLint plane_z_order = static_cast<GLint>(c.plane_z_order);
  GLenum plane_transform = static_cast<GLenum>(c.plane_transform);
  GLuint overlay_texture_id = static_cast<GLuint>(c.overlay_texture_id);
  GLint bounds_x = static_cast<GLint>(c.bounds_x);
  GLint bounds_y = static_cast<GLint>(c.bounds_y);
  GLint bounds_width = static_cast<GLint>(c.bounds_width);
  GLint bounds_height = static_cast<GLint>(c.bounds_height);
  GLfloat uv_x = static_cast<GLfloat>(c.uv_x);
  GLfloat uv_y = static_cast<GLfloat>(c.uv_y);
  GLfloat uv_width = static_cast<GLfloat>(c.uv_width);
  GLfloat uv_height = static_cast<GLfloat>(c.uv_height);
  bool enable_blend = static_cast<bool>(c.enable_blend);
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);

  return DoScheduleOverlayPlaneCHROMIUM(
      plane_z_order, plane_transform, overlay_texture_id, bounds_x, bounds_y,
      bounds_width, bounds_height, uv_x, uv_y, uv_width, uv_height,
      enable_blend, gpu_fence_id);
}

error::Error
GLES2DecoderPassthroughImpl::HandleScheduleCALayerSharedStateCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleCALayerSharedStateCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::ScheduleCALayerSharedStateCHROMIUM*>(
          cmd_data);
  GLfloat opacity = static_cast<GLfloat>(c.opacity);
  GLboolean is_clipped = static_cast<GLboolean>(c.is_clipped);
  GLint sorting_context_id = static_cast<GLint>(c.sorting_context_id);
  uint32_t shm_id = c.shm_id;
  uint32_t shm_offset = c.shm_offset;

  // 4 for |clip_rect|, 5 for |rounded_corner_bounds|, 16 for |transform|.
  const GLfloat* mem = GetSharedMemoryAs<const GLfloat*>(shm_id, shm_offset,
                                                         25 * sizeof(GLfloat));
  if (!mem) {
    return error::kOutOfBounds;
  }
  const GLfloat* clip_rect = mem + 0;
  const GLfloat* rounded_corner_bounds = mem + 4;
  const GLfloat* transform = mem + 9;
  return DoScheduleCALayerSharedStateCHROMIUM(opacity, is_clipped, clip_rect,
                                              rounded_corner_bounds,
                                              sorting_context_id, transform);
}

error::Error GLES2DecoderPassthroughImpl::HandleScheduleCALayerCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleCALayerCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ScheduleCALayerCHROMIUM*>(
          cmd_data);
  GLuint contents_texture_id = static_cast<GLint>(c.contents_texture_id);
  GLuint background_color = static_cast<GLuint>(c.background_color);
  GLuint edge_aa_mask = static_cast<GLuint>(c.edge_aa_mask);
  GLenum filter = static_cast<GLenum>(c.filter);
  uint32_t shm_id = c.shm_id;
  uint32_t shm_offset = c.shm_offset;

  const GLfloat* mem = GetSharedMemoryAs<const GLfloat*>(shm_id, shm_offset,
                                                         8 * sizeof(GLfloat));
  if (!mem) {
    return error::kOutOfBounds;
  }
  const GLfloat* contents_rect = mem;
  const GLfloat* bounds_rect = mem + 4;
  return DoScheduleCALayerCHROMIUM(contents_texture_id, contents_rect,
                                   background_color, edge_aa_mask, filter,
                                   bounds_rect);
}

error::Error GLES2DecoderPassthroughImpl::HandleSetColorSpaceMetadataCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetColorSpaceMetadataCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetColorSpaceMetadataCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;
  GLsizei color_space_size = c.color_space_size;
  const char* data = static_cast<const char*>(
      GetAddressAndCheckSize(c.shm_id, c.shm_offset, color_space_size));
  if (!data) {
    return error::kOutOfBounds;
  }

  // Make a copy to reduce the risk of a time of check to time of use attack.
  std::vector<char> color_space_data(data, data + color_space_size);
  base::Pickle color_space_pickle(color_space_data.data(), color_space_size);
  base::PickleIterator iterator(color_space_pickle);
  gfx::ColorSpace color_space;
  if (!IPC::ParamTraits<gfx::ColorSpace>::Read(&color_space_pickle, &iterator,
                                               &color_space)) {
    return error::kOutOfBounds;
  }

  return DoSetColorSpaceMetadataCHROMIUM(texture_id, color_space);
}

error::Error GLES2DecoderPassthroughImpl::HandleGenPathsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GenPathsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GenPathsCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.first_client_id);
  GLsizei range = static_cast<GLsizei>(c.range);

  return DoGenPathsCHROMIUM(path, range);
}

error::Error GLES2DecoderPassthroughImpl::HandleDeletePathsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::DeletePathsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::DeletePathsCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.first_client_id);
  GLsizei range = static_cast<GLsizei>(c.range);

  return DoDeletePathsCHROMIUM(path, range);
}

error::Error GLES2DecoderPassthroughImpl::HandlePathCommandsCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::PathCommandsCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathCommandsCHROMIUM*>(cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLsizei num_commands = static_cast<GLsizei>(c.numCommands);
  GLsizei num_coords = static_cast<GLsizei>(c.numCoords);
  GLenum coord_type = static_cast<GLenum>(c.coordType);
  uint32_t commands_shm_id = c.commands_shm_id;
  uint32_t commands_shm_offset = c.commands_shm_offset;
  uint32_t coords_shm_id = c.coords_shm_id;
  uint32_t coords_shm_offset = c.coords_shm_offset;

  const GLubyte* commands = nullptr;
  if (num_commands > 0) {
    if (commands_shm_id != 0 || commands_shm_offset != 0) {
      commands = GetSharedMemoryAs<const GLubyte*>(
          commands_shm_id, commands_shm_offset, num_commands);
    }
    if (!commands) {
      return error::kOutOfBounds;
    }
  }
  const GLvoid* coords = nullptr;
  GLsizei coords_bufsize = 0;
  if (num_coords > 0) {
    if (coords_shm_id != 0 || coords_shm_offset != 0) {
      unsigned int memory_size = 0;
      coords = GetSharedMemoryAndSizeAs<const GLvoid*>(
          coords_shm_id, coords_shm_offset, 0, &memory_size);
      coords_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!coords) {
      return error::kOutOfBounds;
    }
  }

  return DoPathCommandsCHROMIUM(path, num_commands, commands, num_coords,
                                coord_type, coords, coords_bufsize);
}

error::Error GLES2DecoderPassthroughImpl::HandlePathParameterfCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::PathParameterfCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathParameterfCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat value = static_cast<GLfloat>(c.value);

  return DoPathParameterfCHROMIUM(path, pname, value);
}

error::Error GLES2DecoderPassthroughImpl::HandlePathParameteriCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::PathParameteriCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathParameteriCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint value = static_cast<GLint>(c.value);

  return DoPathParameteriCHROMIUM(path, pname, value);
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;
  const volatile gles2::cmds::StencilFillPathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::StencilFillPathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);

  return DoStencilFillPathCHROMIUM(path, fill_mode, mask);
}

error::Error GLES2DecoderPassthroughImpl::HandleStencilStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::StencilStrokePathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::StencilStrokePathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);

  return DoStencilStrokePathCHROMIUM(path, reference, mask);
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;
  const volatile gles2::cmds::CoverFillPathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverFillPathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);

  return DoCoverFillPathCHROMIUM(path, cover_mode);
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CoverStrokePathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverStrokePathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);

  return DoCoverStrokePathCHROMIUM(path, cover_mode);
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverFillPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering)
    return error::kUnknownCommand;
  const volatile gles2::cmds::StencilThenCoverFillPathCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilThenCoverFillPathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);

  return DoStencilThenCoverFillPathCHROMIUM(path, fill_mode, mask, cover_mode);
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverStrokePathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::StencilThenCoverStrokePathCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilThenCoverStrokePathCHROMIUM*>(
          cmd_data);
  GLuint path = static_cast<GLuint>(c.path);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);

  return DoStencilThenCoverStrokePathCHROMIUM(path, reference, mask,
                                              cover_mode);
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::StencilFillPathInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilFillPathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  uint32_t paths_shm_id = c.paths_shm_id;
  uint32_t paths_shm_offset = c.paths_shm_offset;
  uint32_t transform_values_shm_id = c.transformValues_shm_id;
  uint32_t transform_values_shm_offset = c.transformValues_shm_offset;

  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, 0, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (transform_values_shm_id != 0 || transform_values_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        transform_values_shm_id, transform_values_shm_offset, 0, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  return DoStencilFillPathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, fill_mode,
      mask, transform_type, transform_values, transform_values_bufsize);
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::StencilStrokePathInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::StencilStrokePathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  uint32_t paths_shm_id = c.paths_shm_id;
  uint32_t paths_shm_offset = c.paths_shm_offset;
  uint32_t transform_values_shm_id = c.transformValues_shm_id;
  uint32_t transform_values_shm_offset = c.transformValues_shm_offset;

  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, 0, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (transform_values_shm_id != 0 || transform_values_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        transform_values_shm_id, transform_values_shm_offset, 0, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  return DoStencilStrokePathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, reference,
      mask, transform_type, transform_values, transform_values_bufsize);
}

error::Error GLES2DecoderPassthroughImpl::HandleCoverFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CoverFillPathInstancedCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverFillPathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  uint32_t paths_shm_id = c.paths_shm_id;
  uint32_t paths_shm_offset = c.paths_shm_offset;
  uint32_t transform_values_shm_id = c.transformValues_shm_id;
  uint32_t transform_values_shm_offset = c.transformValues_shm_offset;

  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, 0, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (transform_values_shm_id != 0 || transform_values_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        transform_values_shm_id, transform_values_shm_offset, 0, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  return DoCoverFillPathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      transform_type, transform_values, transform_values_bufsize);
}

error::Error
GLES2DecoderPassthroughImpl::HandleCoverStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CoverStrokePathInstancedCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::CoverStrokePathInstancedCHROMIUM*>(
          cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  uint32_t paths_shm_id = c.paths_shm_id;
  uint32_t paths_shm_offset = c.paths_shm_offset;
  uint32_t transform_values_shm_id = c.transformValues_shm_id;
  uint32_t transform_values_shm_offset = c.transformValues_shm_offset;

  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, 0, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (transform_values_shm_id != 0 || transform_values_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        transform_values_shm_id, transform_values_shm_offset, 0, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  return DoCoverStrokePathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      transform_type, transform_values, transform_values_bufsize);
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverFillPathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::StencilThenCoverFillPathInstancedCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       StencilThenCoverFillPathInstancedCHROMIUM*>(cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLenum fill_mode = static_cast<GLenum>(c.fillMode);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  uint32_t paths_shm_id = c.paths_shm_id;
  uint32_t paths_shm_offset = c.paths_shm_offset;
  uint32_t transform_values_shm_id = c.transformValues_shm_id;
  uint32_t transform_values_shm_offset = c.transformValues_shm_offset;

  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, 0, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (transform_values_shm_id != 0 || transform_values_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        transform_values_shm_id, transform_values_shm_offset, 0, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  return DoStencilThenCoverFillPathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      fill_mode, mask, transform_type, transform_values,
      transform_values_bufsize);
}

error::Error
GLES2DecoderPassthroughImpl::HandleStencilThenCoverStrokePathInstancedCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::StencilThenCoverStrokePathInstancedCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       StencilThenCoverStrokePathInstancedCHROMIUM*>(cmd_data);
  GLsizei num_paths = static_cast<GLsizei>(c.numPaths);
  GLenum path_name_type = static_cast<GLuint>(c.pathNameType);
  GLuint path_base = static_cast<GLuint>(c.pathBase);
  GLenum cover_mode = static_cast<GLenum>(c.coverMode);
  GLint reference = static_cast<GLint>(c.reference);
  GLuint mask = static_cast<GLuint>(c.mask);
  GLenum transform_type = static_cast<GLuint>(c.transformType);
  uint32_t paths_shm_id = c.paths_shm_id;
  uint32_t paths_shm_offset = c.paths_shm_offset;
  uint32_t transform_values_shm_id = c.transformValues_shm_id;
  uint32_t transform_values_shm_offset = c.transformValues_shm_offset;

  const GLvoid* paths = nullptr;
  GLsizei paths_bufsize = 0;
  if (num_paths > 0) {
    if (paths_shm_id != 0 || paths_shm_offset != 0) {
      unsigned int memory_size = 0;
      paths = GetSharedMemoryAndSizeAs<const GLvoid*>(
          paths_shm_id, paths_shm_offset, 0, &memory_size);
      paths_bufsize = static_cast<GLsizei>(memory_size);
    }

    if (!paths) {
      return error::kOutOfBounds;
    }
  }
  const GLfloat* transform_values = nullptr;
  GLsizei transform_values_bufsize = 0;
  if (transform_values_shm_id != 0 || transform_values_shm_offset != 0) {
    unsigned int memory_size = 0;
    transform_values = GetSharedMemoryAndSizeAs<const GLfloat*>(
        transform_values_shm_id, transform_values_shm_offset, 0, &memory_size);
    transform_values_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!transform_values) {
    return error::kOutOfBounds;
  }

  return DoStencilThenCoverStrokePathInstancedCHROMIUM(
      num_paths, path_name_type, paths, paths_bufsize, path_base, cover_mode,
      reference, mask, transform_type, transform_values,
      transform_values_bufsize);
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindFragmentInputLocationCHROMIUMBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragmentInputLocationCHROMIUMBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindFragmentInputLocationCHROMIUMBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragmentInputLocationCHROMIUM(program, location,
                                             name_str.c_str());
}

error::Error
GLES2DecoderPassthroughImpl::HandleProgramPathFragmentInputGenCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::ProgramPathFragmentInputGenCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::ProgramPathFragmentInputGenCHROMIUM*>(
          cmd_data);
  GLint program = static_cast<GLint>(c.program);
  GLint location = static_cast<GLint>(c.location);
  GLenum gen_mode = static_cast<GLint>(c.genMode);
  GLint components = static_cast<GLint>(c.components);
  uint32_t coeffs_shm_id = c.coeffs_shm_id;
  uint32_t coeffs_shm_offset = c.coeffs_shm_offset;

  const GLfloat* coeffs = nullptr;
  GLsizei coeffs_bufsize = 0;
  if (coeffs_shm_id != 0 || coeffs_shm_offset != 0) {
    unsigned int memory_size = 0;
    coeffs = GetSharedMemoryAndSizeAs<const GLfloat*>(
        coeffs_shm_id, coeffs_shm_offset, 0, &memory_size);
    coeffs_bufsize = static_cast<GLsizei>(memory_size);
  }
  if (!coeffs) {
    return error::kOutOfBounds;
  }
  return DoProgramPathFragmentInputGenCHROMIUM(
      program, location, gen_mode, components, coeffs, coeffs_bufsize);
}

error::Error
GLES2DecoderPassthroughImpl::HandleBindFragDataLocationIndexedEXTBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragDataLocationIndexedEXTBucket& c =
      *static_cast<
          const volatile gles2::cmds::BindFragDataLocationIndexedEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  GLuint index = static_cast<GLuint>(c.index);
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragDataLocationIndexedEXT(program, colorNumber, index,
                                          name_str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleBindFragDataLocationEXTBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::BindFragDataLocationEXTBucket& c =
      *static_cast<const volatile gles2::cmds::BindFragDataLocationEXTBucket*>(
          cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  GLuint colorNumber = static_cast<GLuint>(c.colorNumber);
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  return DoBindFragDataLocationEXT(program, colorNumber, name_str.c_str());
}

error::Error GLES2DecoderPassthroughImpl::HandleGetFragDataIndexEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().ext_blend_func_extended) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::GetFragDataIndexEXT& c =
      *static_cast<const volatile gles2::cmds::GetFragDataIndexEXT*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);
  uint32_t index_shm_id = c.index_shm_id;
  uint32_t index_shm_offset = c.index_shm_offset;
  uint32_t name_bucket_id = c.name_bucket_id;

  Bucket* bucket = GetBucket(name_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  std::string name_str;
  if (!bucket->GetAsString(&name_str)) {
    return error::kInvalidArguments;
  }
  GLint* index =
      GetSharedMemoryAs<GLint*>(index_shm_id, index_shm_offset, sizeof(GLint));
  if (!index) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (*index != -1) {
    return error::kInvalidArguments;
  }
  return DoGetFragDataIndexEXT(program, name_str.c_str(), index);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexImage2DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexImage2DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage2DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  uint32_t bucket_id = c.bucket_id;
  GLint border = static_cast<GLint>(c.border);

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexImage2D(target, level, internal_format, width, height,
                                border, image_size, image_size, data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexImage2D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexImage2D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  unsigned int data_size = 0;
  const void* data = nullptr;
  if (data_shm_id != 0) {
    data = GetSharedMemoryAndSizeAs<const void*>(data_shm_id, data_shm_offset,
                                                 image_size, &data_size);
    if (data == nullptr) {
      return error::kOutOfBounds;
    }
  } else {
    data =
        reinterpret_cast<const void*>(static_cast<intptr_t>(data_shm_offset));
  }

  return DoCompressedTexImage2D(target, level, internal_format, width, height,
                                border, image_size, data_size, data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexSubImage2DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexSubImage2DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage2DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                   height, format, image_size, image_size,
                                   data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexSubImage2D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  const volatile gles2::cmds::CompressedTexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage2D*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  unsigned int data_size = 0;
  const void* data = nullptr;
  if (data_shm_id != 0) {
    data = GetSharedMemoryAndSizeAs<const void*>(data_shm_id, data_shm_offset,
                                                 image_size, &data_size);
    if (data == nullptr) {
      return error::kOutOfBounds;
    }
  } else {
    data =
        reinterpret_cast<const void*>(static_cast<intptr_t>(data_shm_offset));
  }

  return DoCompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                   height, format, image_size, data_size, data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexImage3DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CompressedTexImage3DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage3DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  uint32_t bucket_id = c.bucket_id;
  GLint border = static_cast<GLint>(c.border);

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexImage3D(target, level, internal_format, width, height,
                                depth, border, image_size, image_size, data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexImage3D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CompressedTexImage3D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexImage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internal_format = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLint border = static_cast<GLint>(c.border);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  unsigned int data_size = 0;
  const void* data = nullptr;
  if (data_shm_id != 0) {
    data = GetSharedMemoryAndSizeAs<const void*>(data_shm_id, data_shm_offset,
                                                 image_size, &data_size);
    if (data == nullptr) {
      return error::kOutOfBounds;
    }
  } else {
    data =
        reinterpret_cast<const void*>(static_cast<intptr_t>(data_shm_offset));
  }

  return DoCompressedTexImage3D(target, level, internal_format, width, height,
                                depth, border, image_size, data_size, data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexSubImage3DBucket(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CompressedTexSubImage3DBucket& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage3DBucket*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  uint32_t bucket_id = c.bucket_id;

  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  uint32_t image_size = bucket->size();
  const void* data = bucket->GetData(0, image_size);
  DCHECK(data || !image_size);
  return DoCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                   width, height, depth, format, image_size,
                                   image_size, data);
}

error::Error GLES2DecoderPassthroughImpl::HandleCompressedTexSubImage3D(
    uint32_t immediate_data_size, const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext()) {
    return error::kUnknownCommand;
  }
  const volatile gles2::cmds::CompressedTexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::CompressedTexSubImage3D*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  GLenum format = static_cast<GLenum>(c.format);
  GLsizei image_size = static_cast<GLsizei>(c.imageSize);
  uint32_t data_shm_id = c.data_shm_id;
  uint32_t data_shm_offset = c.data_shm_offset;

  unsigned int data_size = 0;
  const void* data = nullptr;
  if (data_shm_id != 0) {
    data = GetSharedMemoryAndSizeAs<const void*>(data_shm_id, data_shm_offset,
                                                 image_size, &data_size);
    if (data == nullptr) {
      return error::kOutOfBounds;
    }
  } else {
    data =
        reinterpret_cast<const void*>(static_cast<intptr_t>(data_shm_offset));
  }

  return DoCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                   width, height, depth, format, image_size,
                                   data_size, data);
}

error::Error
GLES2DecoderPassthroughImpl::HandleInitializeDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InitializeDiscardableTextureCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::InitializeDiscardableTextureCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;
  uint32_t shm_id = c.shm_id;
  uint32_t shm_offset = c.shm_offset;

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(shm_id);
  if (!DiscardableHandleBase::ValidateParameters(buffer.get(), shm_offset)) {
    return error::kInvalidArguments;
  }

  ServiceDiscardableHandle handle(std::move(buffer), shm_offset, shm_id);

  return DoInitializeDiscardableTextureCHROMIUM(texture_id, std::move(handle));
}

error::Error
GLES2DecoderPassthroughImpl::HandleUnlockDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UnlockDiscardableTextureCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::UnlockDiscardableTextureCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;

  return DoUnlockDiscardableTextureCHROMIUM(texture_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleLockDiscardableTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LockDiscardableTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::LockDiscardableTextureCHROMIUM*>(
          cmd_data);
  GLuint texture_id = c.texture_id;

  return DoLockDiscardableTextureCHROMIUM(texture_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleCreateGpuFenceINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateGpuFenceINTERNAL& c =
      *static_cast<const volatile gles2::cmds::CreateGpuFenceINTERNAL*>(
          cmd_data);
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);
  return DoCreateGpuFenceINTERNAL(gpu_fence_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleWaitGpuFenceCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::WaitGpuFenceCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::WaitGpuFenceCHROMIUM*>(cmd_data);
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);
  return DoWaitGpuFenceCHROMIUM(gpu_fence_id);
}

error::Error GLES2DecoderPassthroughImpl::HandleDestroyGpuFenceCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DestroyGpuFenceCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::DestroyGpuFenceCHROMIUM*>(
          cmd_data);
  GLuint gpu_fence_id = static_cast<GLuint>(c.gpu_fence_id);
  return DoDestroyGpuFenceCHROMIUM(gpu_fence_id);
}

}  // namespace gles2
}  // namespace gpu
