// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the GLES2 command buffer commands.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/common_cmd_format.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/command_buffer/common/gles2_cmd_ids.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

// Command buffer is GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT byte aligned.
#pragma pack(push, 4)
static_assert(GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT == 4,
              "pragma pack alignment must be equal to "
              "GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT");

namespace id_namespaces {

// These are used when contexts share resources.
enum class SharedIdNamespaces {
  kBuffers,
  kProgramsAndShaders,
  kRenderbuffers,
  kTextures,
  kSamplers,
  kSyncs,
  kNumSharedIdNamespaces
};

enum class IdNamespaces {
  kFramebuffers,
  kQueries,
  kVertexArrays,
  kTransformFeedbacks,
  kGpuFences,
  kNumIdNamespaces
};

enum RangeIdNamespaces { kNumRangeIdNamespaces = 1 };

// These numbers must not change
static_assert(static_cast<int>(SharedIdNamespaces::kBuffers) == 0,
              "kBuffers should equal 0");
static_assert(static_cast<int>(SharedIdNamespaces::kProgramsAndShaders) == 1,
              "kProgramsAndShaders should equal 1");
static_assert(static_cast<int>(SharedIdNamespaces::kRenderbuffers) == 2,
              "kRenderbuffers should equal 2");
static_assert(static_cast<int>(SharedIdNamespaces::kTextures) == 3,
              "kTextures should equal 3");
static_assert(static_cast<int>(SharedIdNamespaces::kSamplers) == 4,
              "kSamplers should equal 4");
static_assert(static_cast<int>(SharedIdNamespaces::kSyncs) == 5,
              "kProgramsAndShaders should equal 5");
static_assert(static_cast<int>(IdNamespaces::kFramebuffers) == 0,
              "kFramebuffers should equal 0");
static_assert(static_cast<int>(IdNamespaces::kQueries) == 1,
              "kQueries should equal 1");
static_assert(static_cast<int>(IdNamespaces::kVertexArrays) == 2,
              "kVertexArrays should equal 2");
static_assert(static_cast<int>(IdNamespaces::kTransformFeedbacks) == 3,
              "kTransformFeedbacks should equal 3");
static_assert(static_cast<int>(IdNamespaces::kGpuFences) == 4,
              "kGpuFences should equal 4");
static_assert(kNumRangeIdNamespaces == 1,
              "kNumRangeIdNamespaces should equal 1");

}  // namespace id_namespaces

// The data for one attrib or uniform from GetProgramInfoCHROMIUM.
struct ProgramInput {
  uint32_t type;             // The type (GL_VEC3, GL_MAT3, GL_SAMPLER_2D, etc.
  int32_t size;              // The size (how big the array is for uniforms)
  uint32_t location_offset;  // offset from ProgramInfoHeader to 'size'
                             // locations for uniforms, 1 for attribs.
  uint32_t name_offset;      // offset from ProgrmaInfoHeader to start of name.
  uint32_t name_length;      // length of the name.
};

// The format of the bucket filled out by GetProgramInfoCHROMIUM
struct ProgramInfoHeader {
  uint32_t link_status;
  uint32_t num_attribs;
  uint32_t num_uniforms;
  // ProgramInput inputs[num_attribs + num_uniforms];
};

// The data for one UniformBlock from GetProgramInfoCHROMIUM
struct UniformBlockInfo {
  uint32_t binding;  // UNIFORM_BLOCK_BINDING
  uint32_t data_size;  // UNIFORM_BLOCK_DATA_SIZE
  uint32_t name_offset;  // offset from UniformBlocksHeader to start of name.
  uint32_t name_length;  // UNIFORM_BLOCK_NAME_LENGTH
  uint32_t active_uniforms;  // UNIFORM_BLOCK_ACTIVE_UNIFORMS
  // offset from UniformBlocksHeader to |active_uniforms| indices.
  uint32_t active_uniform_offset;
  // UNIFORM_BLOCK_REFERENDED_BY_VERTEX_SHADER
  uint32_t referenced_by_vertex_shader;
  // UNIFORM_BLOCK_REFERENDED_BY_FRAGMENT_SHADER
  uint32_t referenced_by_fragment_shader;
};

// The format of the bucket filled out by GetUniformBlocksCHROMIUM
struct UniformBlocksHeader {
  uint32_t num_uniform_blocks;
  // UniformBlockInfo uniform_blocks[num_uniform_blocks];
};

// The data for one TransformFeedbackVarying from
// GetTransformFeedbackVaringCHROMIUM.
struct TransformFeedbackVaryingInfo {
  uint32_t size;
  uint32_t type;
  uint32_t name_offset;  // offset from Header to start of name.
  uint32_t name_length;  // including the null terminator.
};

// The format of the bucket filled out by GetTransformFeedbackVaryingsCHROMIUM
struct TransformFeedbackVaryingsHeader {
  uint32_t transform_feedback_buffer_mode;
  uint32_t num_transform_feedback_varyings;
  // TransformFeedbackVaryingInfo varyings[num_transform_feedback_varyings];
};

// Parameters of a uniform that can be queried through glGetActiveUniformsiv,
// but not through glGetActiveUniform.
struct UniformES3Info {
  int32_t block_index;
  int32_t offset;
  int32_t array_stride;
  int32_t matrix_stride;
  int32_t is_row_major;
};

// The format of the bucket filled out by GetUniformsivES3CHROMIUM
struct UniformsES3Header {
  uint32_t num_uniforms;
  // UniformES3Info uniforms[num_uniforms];
};

struct DisjointValueSync {
  void Reset() {
    base::subtle::Release_Store(&disjoint_count, 0);
  }

  void SetDisjointCount(uint32_t token) {
    DCHECK_NE(token, 0u);
    base::subtle::Release_Store(&disjoint_count, token);
  }

  uint32_t GetDisjointCount() {
    return base::subtle::Acquire_Load(&disjoint_count);
  }

  base::subtle::Atomic32 disjoint_count;
};

static_assert(sizeof(QuerySync) == 12, "size of QuerySync should be 12");
static_assert(offsetof(QuerySync, process_count) == 0,
              "offset of QuerySync.process_count should be 0");
static_assert(offsetof(QuerySync, result) == 4,
              "offset of QuerySync.result should be 4");

static_assert(sizeof(ProgramInput) == 20, "size of ProgramInput should be 20");
static_assert(offsetof(ProgramInput, type) == 0,
              "offset of ProgramInput.type should be 0");
static_assert(offsetof(ProgramInput, size) == 4,
              "offset of ProgramInput.size should be 4");
static_assert(offsetof(ProgramInput, location_offset) == 8,
              "offset of ProgramInput.location_offset should be 8");
static_assert(offsetof(ProgramInput, name_offset) == 12,
              "offset of ProgramInput.name_offset should be 12");
static_assert(offsetof(ProgramInput, name_length) == 16,
              "offset of ProgramInput.name_length should be 16");

static_assert(sizeof(ProgramInfoHeader) == 12,
              "size of ProgramInfoHeader should be 12");
static_assert(offsetof(ProgramInfoHeader, link_status) == 0,
              "offset of ProgramInfoHeader.link_status should be 0");
static_assert(offsetof(ProgramInfoHeader, num_attribs) == 4,
              "offset of ProgramInfoHeader.num_attribs should be 4");
static_assert(offsetof(ProgramInfoHeader, num_uniforms) == 8,
              "offset of ProgramInfoHeader.num_uniforms should be 8");

static_assert(sizeof(UniformBlockInfo) == 32,
              "size of UniformBlockInfo should be 32");
static_assert(offsetof(UniformBlockInfo, binding) == 0,
              "offset of UniformBlockInfo.binding should be 0");
static_assert(offsetof(UniformBlockInfo, data_size) == 4,
              "offset of UniformBlockInfo.data_size should be 4");
static_assert(offsetof(UniformBlockInfo, name_offset) == 8,
              "offset of UniformBlockInfo.name_offset should be 8");
static_assert(offsetof(UniformBlockInfo, name_length) == 12,
              "offset of UniformBlockInfo.name_length should be 12");
static_assert(offsetof(UniformBlockInfo, active_uniforms) == 16,
              "offset of UniformBlockInfo.active_uniforms should be 16");
static_assert(offsetof(UniformBlockInfo, active_uniform_offset) == 20,
              "offset of UniformBlockInfo.active_uniform_offset should be 20");
static_assert(offsetof(UniformBlockInfo, referenced_by_vertex_shader) == 24,
              "offset of UniformBlockInfo.referenced_by_vertex_shader "
              "should be 24");
static_assert(offsetof(UniformBlockInfo, referenced_by_fragment_shader) == 28,
              "offset of UniformBlockInfo.referenced_by_fragment_shader "
              "should be 28");

static_assert(sizeof(UniformBlocksHeader) == 4,
              "size of UniformBlocksHeader should be 4");
static_assert(offsetof(UniformBlocksHeader, num_uniform_blocks) == 0,
              "offset of UniformBlocksHeader.num_uniform_blocks should be 0");

enum class GLES2ReturnDataType : uint32_t {
  kES2ProgramInfo,
  kES3UniformBlocks,
  kES3TransformFeedbackVaryings,
  kES3Uniforms
};

namespace cmds {

#include "gpu/command_buffer/common/gles2_cmd_format_autogen.h"

struct GLES2ReturnDataHeader {
  GLES2ReturnDataType return_data_type;
};
static_assert(sizeof(GLES2ReturnDataHeader) == 4,
              "size of GLES2ReturnDataHeader should be 4");
static_assert(offsetof(GLES2ReturnDataHeader, return_data_type) == 0,
              "The offset of return_data_type should be 0");

struct GLES2ReturnProgramInfo {
  GLES2ReturnDataHeader return_data_header = {
      GLES2ReturnDataType::kES2ProgramInfo};
  uint32_t program_client_id = 0;
  char deserialized_buffer[];
};
static_assert(sizeof(GLES2ReturnProgramInfo) == 8,
              "size of GLES2ReturnProgramInfo should be 8");
static_assert(offsetof(GLES2ReturnProgramInfo, return_data_header) == 0,
              "The offset of return_data_header should be 0");
static_assert(offsetof(GLES2ReturnProgramInfo, program_client_id) == 4,
              "The offset of program_client_id should be 4");
static_assert(offsetof(GLES2ReturnProgramInfo, deserialized_buffer) == 8,
              "The offset of deserialized_buffer should be 8");

#pragma pack(pop)

}  // namespace cmd
}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H_
