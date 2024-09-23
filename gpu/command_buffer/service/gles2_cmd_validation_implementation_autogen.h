// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_

static const GLenum valid_attachment_table[] = {
    GL_COLOR_ATTACHMENT0,
    GL_DEPTH_ATTACHMENT,
    GL_STENCIL_ATTACHMENT,
};

static const GLenum valid_attachment_table_es3[] = {
    GL_DEPTH_STENCIL_ATTACHMENT,
};

static const GLenum valid_attachment_query_table[] = {
    GL_COLOR_ATTACHMENT0,
    GL_DEPTH_ATTACHMENT,
    GL_STENCIL_ATTACHMENT,
};

static const GLenum valid_attachment_query_table_es3[] = {
    GL_DEPTH_STENCIL_ATTACHMENT,
    GL_COLOR_EXT,
    GL_DEPTH_EXT,
    GL_STENCIL_EXT,
};

bool Validators::BackbufferAttachmentValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_COLOR_EXT:
    case GL_DEPTH_EXT:
    case GL_STENCIL_EXT:
      return true;
  }
  return false;
}

bool Validators::BlitFilterValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_NEAREST:
    case GL_LINEAR:
      return true;
  }
  return false;
}

bool Validators::BufferModeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_INTERLEAVED_ATTRIBS:
    case GL_SEPARATE_ATTRIBS:
      return true;
  }
  return false;
}

Validators::BufferParameterValidator::BufferParameterValidator()
    : is_es3_(false) {}
bool Validators::BufferParameterValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_BUFFER_SIZE:
    case GL_BUFFER_USAGE:
      return true;
    case GL_BUFFER_ACCESS_FLAGS:
    case GL_BUFFER_MAPPED:
      return is_es3_;
  }
  return false;
}

bool Validators::BufferParameter64Validator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_BUFFER_SIZE:
    case GL_BUFFER_MAP_LENGTH:
    case GL_BUFFER_MAP_OFFSET:
      return true;
  }
  return false;
}

Validators::BufferTargetValidator::BufferTargetValidator() : is_es3_(false) {}
bool Validators::BufferTargetValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_ARRAY_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
      return true;
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
      return is_es3_;
  }
  return false;
}

Validators::BufferUsageValidator::BufferUsageValidator() : is_es3_(false) {}
bool Validators::BufferUsageValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_STREAM_DRAW:
    case GL_STATIC_DRAW:
    case GL_DYNAMIC_DRAW:
      return true;
    case GL_STREAM_READ:
    case GL_STREAM_COPY:
    case GL_STATIC_READ:
    case GL_STATIC_COPY:
    case GL_DYNAMIC_READ:
    case GL_DYNAMIC_COPY:
      return is_es3_;
  }
  return false;
}

static const GLenum valid_bufferfi_table[] = {
    GL_DEPTH_STENCIL,
};

bool Validators::BufferfvValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_COLOR:
    case GL_DEPTH:
      return true;
  }
  return false;
}

bool Validators::BufferivValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_COLOR:
    case GL_STENCIL:
      return true;
  }
  return false;
}

static const GLenum valid_bufferuiv_table[] = {
    GL_COLOR,
};

static const GLenum valid_capability_table[] = {
    GL_BLEND,           GL_CULL_FACE,           GL_DEPTH_TEST,
    GL_DITHER,          GL_POLYGON_OFFSET_FILL, GL_SAMPLE_ALPHA_TO_COVERAGE,
    GL_SAMPLE_COVERAGE, GL_SCISSOR_TEST,        GL_STENCIL_TEST,
};

static const GLenum valid_capability_table_es3[] = {
    GL_RASTERIZER_DISCARD,
    GL_PRIMITIVE_RESTART_FIXED_INDEX,
};

bool Validators::CmpFunctionValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_NEVER:
    case GL_LESS:
    case GL_EQUAL:
    case GL_LEQUAL:
    case GL_GREATER:
    case GL_NOTEQUAL:
    case GL_GEQUAL:
    case GL_ALWAYS:
      return true;
  }
  return false;
}

bool Validators::DrawModeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_POINTS:
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_LINES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_TRIANGLES:
      return true;
  }
  return false;
}

static const GLenum valid_dst_blend_factor_table[] = {
    GL_ZERO,           GL_ONE,
    GL_SRC_COLOR,      GL_ONE_MINUS_SRC_COLOR,
    GL_DST_COLOR,      GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,      GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,      GL_ONE_MINUS_DST_ALPHA,
    GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA,
};

static const GLenum valid_dst_blend_factor_table_es3[] = {
    GL_SRC_ALPHA_SATURATE,
};

static const GLenum valid_equation_table[] = {
    GL_FUNC_ADD,
    GL_FUNC_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT,
};

static const GLenum valid_equation_table_es3[] = {
    GL_MIN,
    GL_MAX,
};

bool Validators::FaceModeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_CW:
    case GL_CCW:
      return true;
  }
  return false;
}

bool Validators::FaceTypeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_FRONT:
    case GL_BACK:
    case GL_FRONT_AND_BACK:
      return true;
  }
  return false;
}

static const GLenum valid_framebuffer_attachment_parameter_table[] = {
    GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
    GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
    GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
    GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
};

static const GLenum valid_framebuffer_attachment_parameter_table_es3[] = {
    GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,
    GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
    GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,
    GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
    GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
    GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
    GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,
    GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
    GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
};

static const GLenum valid_framebuffer_target_table[] = {
    GL_FRAMEBUFFER,
};

static const GLenum valid_framebuffer_target_table_es3[] = {
    GL_DRAW_FRAMEBUFFER,
    GL_READ_FRAMEBUFFER,
};

static const GLenum valid_g_l_state_table[] = {
    GL_ACTIVE_TEXTURE,
    GL_ALIASED_LINE_WIDTH_RANGE,
    GL_ALIASED_POINT_SIZE_RANGE,
    GL_ALPHA_BITS,
    GL_ARRAY_BUFFER_BINDING,
    GL_BLUE_BITS,
    GL_COMPRESSED_TEXTURE_FORMATS,
    GL_CURRENT_PROGRAM,
    GL_DEPTH_BITS,
    GL_DEPTH_RANGE,
    GL_ELEMENT_ARRAY_BUFFER_BINDING,
    GL_FRAMEBUFFER_BINDING,
    GL_GENERATE_MIPMAP_HINT,
    GL_GREEN_BITS,
    GL_IMPLEMENTATION_COLOR_READ_FORMAT,
    GL_IMPLEMENTATION_COLOR_READ_TYPE,
    GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
    GL_MAX_CUBE_MAP_TEXTURE_SIZE,
    GL_MAX_FRAGMENT_UNIFORM_VECTORS,
    GL_MAX_RENDERBUFFER_SIZE,
    GL_MAX_TEXTURE_IMAGE_UNITS,
    GL_MAX_TEXTURE_SIZE,
    GL_MAX_VARYING_VECTORS,
    GL_MAX_VERTEX_ATTRIBS,
    GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
    GL_MAX_VERTEX_UNIFORM_VECTORS,
    GL_MAX_VIEWPORT_DIMS,
    GL_NUM_COMPRESSED_TEXTURE_FORMATS,
    GL_NUM_SHADER_BINARY_FORMATS,
    GL_PACK_ALIGNMENT,
    GL_RED_BITS,
    GL_RENDERBUFFER_BINDING,
    GL_SAMPLE_BUFFERS,
    GL_SAMPLE_COVERAGE_INVERT,
    GL_SAMPLE_COVERAGE_VALUE,
    GL_SAMPLES,
    GL_SCISSOR_BOX,
    GL_SHADER_BINARY_FORMATS,
    GL_SHADER_COMPILER,
    GL_SUBPIXEL_BITS,
    GL_STENCIL_BITS,
    GL_TEXTURE_BINDING_2D,
    GL_TEXTURE_BINDING_CUBE_MAP,
    GL_UNPACK_ALIGNMENT,
    GL_BIND_GENERATES_RESOURCE_CHROMIUM,
    GL_VERTEX_ARRAY_BINDING_OES,
    GL_VIEWPORT,
    GL_BLEND_COLOR,
    GL_BLEND_EQUATION_RGB,
    GL_BLEND_EQUATION_ALPHA,
    GL_BLEND_SRC_RGB,
    GL_BLEND_DST_RGB,
    GL_BLEND_SRC_ALPHA,
    GL_BLEND_DST_ALPHA,
    GL_COLOR_CLEAR_VALUE,
    GL_DEPTH_CLEAR_VALUE,
    GL_STENCIL_CLEAR_VALUE,
    GL_COLOR_WRITEMASK,
    GL_CULL_FACE_MODE,
    GL_DEPTH_FUNC,
    GL_DEPTH_WRITEMASK,
    GL_FRONT_FACE,
    GL_LINE_WIDTH,
    GL_POLYGON_OFFSET_FACTOR,
    GL_POLYGON_OFFSET_UNITS,
    GL_STENCIL_FUNC,
    GL_STENCIL_REF,
    GL_STENCIL_VALUE_MASK,
    GL_STENCIL_BACK_FUNC,
    GL_STENCIL_BACK_REF,
    GL_STENCIL_BACK_VALUE_MASK,
    GL_STENCIL_WRITEMASK,
    GL_STENCIL_BACK_WRITEMASK,
    GL_STENCIL_FAIL,
    GL_STENCIL_PASS_DEPTH_FAIL,
    GL_STENCIL_PASS_DEPTH_PASS,
    GL_STENCIL_BACK_FAIL,
    GL_STENCIL_BACK_PASS_DEPTH_FAIL,
    GL_STENCIL_BACK_PASS_DEPTH_PASS,
    GL_BLEND,
    GL_CULL_FACE,
    GL_DEPTH_TEST,
    GL_DITHER,
    GL_POLYGON_OFFSET_FILL,
    GL_SAMPLE_ALPHA_TO_COVERAGE,
    GL_SAMPLE_COVERAGE,
    GL_SCISSOR_TEST,
    GL_STENCIL_TEST,
    GL_RASTERIZER_DISCARD,
    GL_PRIMITIVE_RESTART_FIXED_INDEX,
};

static const GLenum valid_g_l_state_table_es3[] = {
    GL_COPY_READ_BUFFER_BINDING,
    GL_COPY_WRITE_BUFFER_BINDING,
    GL_DRAW_BUFFER0,
    GL_DRAW_BUFFER1,
    GL_DRAW_BUFFER2,
    GL_DRAW_BUFFER3,
    GL_DRAW_BUFFER4,
    GL_DRAW_BUFFER5,
    GL_DRAW_BUFFER6,
    GL_DRAW_BUFFER7,
    GL_DRAW_BUFFER8,
    GL_DRAW_BUFFER9,
    GL_DRAW_BUFFER10,
    GL_DRAW_BUFFER11,
    GL_DRAW_BUFFER12,
    GL_DRAW_BUFFER13,
    GL_DRAW_BUFFER14,
    GL_DRAW_BUFFER15,
    GL_DRAW_FRAMEBUFFER_BINDING,
    GL_FRAGMENT_SHADER_DERIVATIVE_HINT,
    GL_GPU_DISJOINT_EXT,
    GL_MAJOR_VERSION,
    GL_MAX_3D_TEXTURE_SIZE,
    GL_MAX_ARRAY_TEXTURE_LAYERS,
    GL_MAX_COLOR_ATTACHMENTS,
    GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
    GL_MAX_COMBINED_UNIFORM_BLOCKS,
    GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
    GL_MAX_DRAW_BUFFERS,
    GL_MAX_ELEMENT_INDEX,
    GL_MAX_ELEMENTS_INDICES,
    GL_MAX_ELEMENTS_VERTICES,
    GL_MAX_FRAGMENT_INPUT_COMPONENTS,
    GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
    GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
    GL_MAX_PROGRAM_TEXEL_OFFSET,
    GL_MAX_SAMPLES,
    GL_MAX_SERVER_WAIT_TIMEOUT,
    GL_MAX_TEXTURE_LOD_BIAS,
    GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
    GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
    GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
    GL_MAX_UNIFORM_BLOCK_SIZE,
    GL_MAX_UNIFORM_BUFFER_BINDINGS,
    GL_MAX_VARYING_COMPONENTS,
    GL_MAX_VERTEX_OUTPUT_COMPONENTS,
    GL_MAX_VERTEX_UNIFORM_BLOCKS,
    GL_MAX_VERTEX_UNIFORM_COMPONENTS,
    GL_MIN_PROGRAM_TEXEL_OFFSET,
    GL_MINOR_VERSION,
    GL_NUM_EXTENSIONS,
    GL_NUM_PROGRAM_BINARY_FORMATS,
    GL_PACK_ROW_LENGTH,
    GL_PACK_SKIP_PIXELS,
    GL_PACK_SKIP_ROWS,
    GL_PIXEL_PACK_BUFFER_BINDING,
    GL_PIXEL_UNPACK_BUFFER_BINDING,
    GL_PROGRAM_BINARY_FORMATS,
    GL_READ_BUFFER,
    GL_READ_FRAMEBUFFER_BINDING,
    GL_SAMPLER_BINDING,
    GL_TIMESTAMP_EXT,
    GL_TEXTURE_BINDING_2D_ARRAY,
    GL_TEXTURE_BINDING_3D,
    GL_TRANSFORM_FEEDBACK_BINDING,
    GL_TRANSFORM_FEEDBACK_ACTIVE,
    GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
    GL_TRANSFORM_FEEDBACK_PAUSED,
    GL_UNIFORM_BUFFER_BINDING,
    GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
    GL_UNPACK_IMAGE_HEIGHT,
    GL_UNPACK_ROW_LENGTH,
    GL_UNPACK_SKIP_IMAGES,
    GL_UNPACK_SKIP_PIXELS,
    GL_UNPACK_SKIP_ROWS,
    GL_BLEND_EQUATION_RGB,
    GL_BLEND_EQUATION_ALPHA,
    GL_BLEND_SRC_RGB,
    GL_BLEND_SRC_ALPHA,
    GL_BLEND_DST_RGB,
    GL_BLEND_DST_ALPHA,
    GL_COLOR_WRITEMASK,
};

bool Validators::GetMaxIndexTypeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_INT:
      return true;
  }
  return false;
}

static const GLenum valid_get_tex_param_target_table[] = {
    GL_TEXTURE_2D,
    GL_TEXTURE_CUBE_MAP,
};

static const GLenum valid_get_tex_param_target_table_es3[] = {
    GL_TEXTURE_2D_ARRAY,
    GL_TEXTURE_3D,
};

bool Validators::HintModeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_FASTEST:
    case GL_NICEST:
    case GL_DONT_CARE:
      return true;
  }
  return false;
}

static const GLenum valid_hint_target_table[] = {
    GL_GENERATE_MIPMAP_HINT,
};

static const GLenum valid_hint_target_table_es3[] = {
    GL_FRAGMENT_SHADER_DERIVATIVE_HINT,
};

static const GLenum valid_image_internal_format_table[] = {
    GL_RGB,
    GL_RGBA,
};

static const GLenum valid_index_type_table[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT,
};

static const GLenum valid_index_type_table_es3[] = {
    GL_UNSIGNED_INT,
};

bool Validators::IndexedBufferTargetValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
      return true;
  }
  return false;
}

static const GLenum valid_indexed_g_l_state_table[] = {
    GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
    GL_TRANSFORM_FEEDBACK_BUFFER_SIZE,
    GL_TRANSFORM_FEEDBACK_BUFFER_START,
    GL_UNIFORM_BUFFER_BINDING,
    GL_UNIFORM_BUFFER_SIZE,
    GL_UNIFORM_BUFFER_START,
    GL_BLEND_EQUATION_RGB,
    GL_BLEND_EQUATION_ALPHA,
    GL_BLEND_SRC_RGB,
    GL_BLEND_SRC_ALPHA,
    GL_BLEND_DST_RGB,
    GL_BLEND_DST_ALPHA,
    GL_COLOR_WRITEMASK,
};

bool Validators::InternalFormatParameterValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_NUM_SAMPLE_COUNTS:
    case GL_SAMPLES:
      return true;
  }
  return false;
}

bool Validators::MapBufferAccessValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_MAP_READ_BIT:
    case GL_MAP_WRITE_BIT:
    case GL_MAP_INVALIDATE_RANGE_BIT:
    case GL_MAP_INVALIDATE_BUFFER_BIT:
    case GL_MAP_FLUSH_EXPLICIT_BIT:
    case GL_MAP_UNSYNCHRONIZED_BIT:
      return true;
  }
  return false;
}

static const GLenum valid_pixel_store_table[] = {
    GL_PACK_ALIGNMENT,
    GL_UNPACK_ALIGNMENT,
};

static const GLenum valid_pixel_store_table_es3[] = {
    GL_PACK_ROW_LENGTH,   GL_PACK_SKIP_PIXELS,    GL_PACK_SKIP_ROWS,
    GL_UNPACK_ROW_LENGTH, GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_PIXELS,
    GL_UNPACK_SKIP_ROWS,  GL_UNPACK_SKIP_IMAGES,
};

bool Validators::PixelStoreAlignmentValidator::IsValid(
    const GLint value) const {
  switch (value) {
    case 1:
    case 2:
    case 4:
    case 8:
      return true;
  }
  return false;
}

static const GLenum valid_pixel_type_table[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT_5_6_5,
    GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

static const GLenum valid_pixel_type_table_es3[] = {
    GL_BYTE,
    GL_UNSIGNED_SHORT,
    GL_SHORT,
    GL_UNSIGNED_INT,
    GL_INT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_INT_2_10_10_10_REV,
    GL_UNSIGNED_INT_10F_11F_11F_REV,
    GL_UNSIGNED_INT_5_9_9_9_REV,
    GL_UNSIGNED_INT_24_8,
    GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
};

static const GLenum valid_program_parameter_table[] = {
    GL_DELETE_STATUS,
    GL_LINK_STATUS,
    GL_VALIDATE_STATUS,
    GL_INFO_LOG_LENGTH,
    GL_ATTACHED_SHADERS,
    GL_ACTIVE_ATTRIBUTES,
    GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
    GL_ACTIVE_UNIFORMS,
    GL_ACTIVE_UNIFORM_MAX_LENGTH,
};

static const GLenum valid_program_parameter_table_es3[] = {
    GL_ACTIVE_UNIFORM_BLOCKS,
    GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
    GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
    GL_TRANSFORM_FEEDBACK_VARYINGS,
    GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
};

bool Validators::QueryObjectParameterValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_QUERY_RESULT_EXT:
    case GL_QUERY_RESULT_AVAILABLE_EXT:
    case GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT:
      return true;
  }
  return false;
}

bool Validators::QueryTargetValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_SAMPLES_PASSED_ARB:
    case GL_ANY_SAMPLES_PASSED_EXT:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
    case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
      return true;
  }
  return false;
}

static const GLenum valid_read_buffer_table[] = {
    GL_NONE,
    GL_BACK,
    GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2,
    GL_COLOR_ATTACHMENT3,
    GL_COLOR_ATTACHMENT4,
    GL_COLOR_ATTACHMENT5,
    GL_COLOR_ATTACHMENT6,
    GL_COLOR_ATTACHMENT7,
    GL_COLOR_ATTACHMENT8,
    GL_COLOR_ATTACHMENT9,
    GL_COLOR_ATTACHMENT10,
    GL_COLOR_ATTACHMENT11,
    GL_COLOR_ATTACHMENT12,
    GL_COLOR_ATTACHMENT13,
    GL_COLOR_ATTACHMENT14,
    GL_COLOR_ATTACHMENT15,
};

static const GLenum valid_read_pixel_format_table[] = {
    GL_ALPHA,
    GL_RGB,
    GL_RGBA,
};

static const GLenum valid_read_pixel_format_table_es3[] = {
    GL_RED,        GL_RED_INTEGER, GL_RG,
    GL_RG_INTEGER, GL_RGB_INTEGER, GL_RGBA_INTEGER,
};

static const GLenum valid_read_pixel_type_table[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT_5_6_5,
    GL_UNSIGNED_SHORT_4_4_4_4,
    GL_UNSIGNED_SHORT_5_5_5_1,
};

static const GLenum valid_read_pixel_type_table_es3[] = {
    GL_BYTE, GL_UNSIGNED_SHORT, GL_SHORT, GL_UNSIGNED_INT,
    GL_INT,  GL_HALF_FLOAT,     GL_FLOAT, GL_UNSIGNED_INT_2_10_10_10_REV,
};

static const GLenum valid_render_buffer_format_table[] = {
    GL_RGBA4, GL_RGB565, GL_RGB5_A1, GL_DEPTH_COMPONENT16, GL_STENCIL_INDEX8,
};

static const GLenum valid_render_buffer_format_table_es3[] = {
    GL_R8,
    GL_R8UI,
    GL_R8I,
    GL_R16UI,
    GL_R16I,
    GL_R32UI,
    GL_R32I,
    GL_RG8,
    GL_RG8UI,
    GL_RG8I,
    GL_RG16UI,
    GL_RG16I,
    GL_RG32UI,
    GL_RG32I,
    GL_RGB8,
    GL_RGBA8,
    GL_SRGB8_ALPHA8,
    GL_RGB10_A2,
    GL_RGBA8UI,
    GL_RGBA8I,
    GL_RGB10_A2UI,
    GL_RGBA16UI,
    GL_RGBA16I,
    GL_RGBA32UI,
    GL_RGBA32I,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

static const GLenum valid_render_buffer_parameter_table[] = {
    GL_RENDERBUFFER_RED_SIZE,        GL_RENDERBUFFER_GREEN_SIZE,
    GL_RENDERBUFFER_BLUE_SIZE,       GL_RENDERBUFFER_ALPHA_SIZE,
    GL_RENDERBUFFER_DEPTH_SIZE,      GL_RENDERBUFFER_STENCIL_SIZE,
    GL_RENDERBUFFER_WIDTH,           GL_RENDERBUFFER_HEIGHT,
    GL_RENDERBUFFER_INTERNAL_FORMAT,
};

static const GLenum valid_render_buffer_parameter_table_es3[] = {
    GL_RENDERBUFFER_SAMPLES,
};

static const GLenum valid_render_buffer_target_table[] = {
    GL_RENDERBUFFER,
};

bool Validators::ResetStatusValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
    case GL_INNOCENT_CONTEXT_RESET_ARB:
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return true;
  }
  return false;
}

static const GLenum valid_sampler_parameter_table[] = {
    GL_TEXTURE_MAG_FILTER, GL_TEXTURE_MIN_FILTER,   GL_TEXTURE_MIN_LOD,
    GL_TEXTURE_MAX_LOD,    GL_TEXTURE_WRAP_S,       GL_TEXTURE_WRAP_T,
    GL_TEXTURE_WRAP_R,     GL_TEXTURE_COMPARE_MODE, GL_TEXTURE_COMPARE_FUNC,
};

static const GLenum valid_shader_parameter_table[] = {
    GL_SHADER_TYPE,          GL_DELETE_STATUS,
    GL_COMPILE_STATUS,       GL_INFO_LOG_LENGTH,
    GL_SHADER_SOURCE_LENGTH, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
};

bool Validators::ShaderPrecisionValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      return true;
  }
  return false;
}

bool Validators::ShaderTypeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_VERTEX_SHADER:
    case GL_FRAGMENT_SHADER:
      return true;
  }
  return false;
}

bool Validators::SharedImageAccessModeValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM:
    case GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM:
      return true;
  }
  return false;
}

static const GLenum valid_src_blend_factor_table[] = {
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_CONSTANT_COLOR,
    GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA,
    GL_ONE_MINUS_CONSTANT_ALPHA,
    GL_SRC_ALPHA_SATURATE,
};

bool Validators::StencilOpValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_KEEP:
    case GL_ZERO:
    case GL_REPLACE:
    case GL_INCR:
    case GL_INCR_WRAP:
    case GL_DECR:
    case GL_DECR_WRAP:
    case GL_INVERT:
      return true;
  }
  return false;
}

bool Validators::StringTypeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_VENDOR:
    case GL_RENDERER:
    case GL_VERSION:
    case GL_SHADING_LANGUAGE_VERSION:
    case GL_EXTENSIONS:
      return true;
  }
  return false;
}

bool Validators::SwapBuffersFlagsValidator::IsValid(
    const GLbitfield value) const {
  switch (value) {
    case 0:
    case gpu::SwapBuffersFlags::kVSyncParams:
      return true;
  }
  return false;
}

static const GLbitfield valid_sync_flush_flags_table[] = {
    GL_SYNC_FLUSH_COMMANDS_BIT,
    0,
};

bool Validators::SyncParameterValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_SYNC_STATUS:
    case GL_OBJECT_TYPE:
    case GL_SYNC_CONDITION:
    case GL_SYNC_FLAGS:
      return true;
  }
  return false;
}

bool Validators::Texture3DTargetValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_TEXTURE_3D:
    case GL_TEXTURE_2D_ARRAY:
      return true;
  }
  return false;
}

static const GLenum valid_texture_bind_target_table[] = {
    GL_TEXTURE_2D,
    GL_TEXTURE_CUBE_MAP,
};

static const GLenum valid_texture_bind_target_table_es3[] = {
    GL_TEXTURE_3D,
    GL_TEXTURE_2D_ARRAY,
};

bool Validators::TextureCompareFuncValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_LEQUAL:
    case GL_GEQUAL:
    case GL_LESS:
    case GL_GREATER:
    case GL_EQUAL:
    case GL_NOTEQUAL:
    case GL_ALWAYS:
    case GL_NEVER:
      return true;
  }
  return false;
}

static const GLenum valid_texture_compare_mode_table[] = {
    GL_NONE,
    GL_COMPARE_REF_TO_TEXTURE,
};

static const GLenum valid_texture_depth_renderable_internal_format_table_es3[] =
    {
        GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F,
        GL_DEPTH24_STENCIL8,  GL_DEPTH32F_STENCIL8,
};

static const GLenum valid_texture_fbo_target_table[] = {
    GL_TEXTURE_2D,
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

static const GLenum valid_texture_format_table[] = {
    GL_ALPHA, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA,
};

static const GLenum valid_texture_format_table_es3[] = {
    GL_RED,
    GL_RED_INTEGER,
    GL_RG,
    GL_RG_INTEGER,
    GL_RGB_INTEGER,
    GL_RGBA_INTEGER,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
};

static const GLenum valid_texture_internal_format_table[] = {
    GL_ALPHA, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA,
};

static const GLenum valid_texture_internal_format_table_es3[] = {
    GL_R8,
    GL_R8_SNORM,
    GL_R16F,
    GL_R32F,
    GL_R8UI,
    GL_R8I,
    GL_R16UI,
    GL_R16I,
    GL_R32UI,
    GL_R32I,
    GL_RG8,
    GL_RG8_SNORM,
    GL_RG16F,
    GL_RG32F,
    GL_RG8UI,
    GL_RG8I,
    GL_RG16UI,
    GL_RG16I,
    GL_RG32UI,
    GL_RG32I,
    GL_RGB8,
    GL_SRGB8,
    GL_RGB565,
    GL_RGB8_SNORM,
    GL_R11F_G11F_B10F,
    GL_RGB9_E5,
    GL_RGB16F,
    GL_RGB32F,
    GL_RGB8UI,
    GL_RGB8I,
    GL_RGB16UI,
    GL_RGB16I,
    GL_RGB32UI,
    GL_RGB32I,
    GL_RGBA8,
    GL_SRGB8_ALPHA8,
    GL_RGBA8_SNORM,
    GL_RGB5_A1,
    GL_RGBA4,
    GL_RGB10_A2,
    GL_RGBA16F,
    GL_RGBA32F,
    GL_RGBA8UI,
    GL_RGBA8I,
    GL_RGB10_A2UI,
    GL_RGBA16UI,
    GL_RGBA16I,
    GL_RGBA32UI,
    GL_RGBA32I,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

static const GLenum valid_texture_internal_format_storage_table[] = {
    GL_RGB565,     GL_RGBA4,          GL_RGB5_A1,
    GL_ALPHA8_EXT, GL_LUMINANCE8_EXT, GL_LUMINANCE8_ALPHA8_EXT,
    GL_RGB8_OES,   GL_RGBA8_OES,
};

static const GLenum valid_texture_internal_format_storage_table_es3[] = {
    GL_R8,
    GL_R8_SNORM,
    GL_R16F,
    GL_R32F,
    GL_R8UI,
    GL_R8I,
    GL_R16UI,
    GL_R16I,
    GL_R32UI,
    GL_R32I,
    GL_RG8,
    GL_RG8_SNORM,
    GL_RG16F,
    GL_RG32F,
    GL_RG8UI,
    GL_RG8I,
    GL_RG16UI,
    GL_RG16I,
    GL_RG32UI,
    GL_RG32I,
    GL_RGB8,
    GL_SRGB8,
    GL_RGB8_SNORM,
    GL_R11F_G11F_B10F,
    GL_RGB9_E5,
    GL_RGB16F,
    GL_RGB32F,
    GL_RGB8UI,
    GL_RGB8I,
    GL_RGB16UI,
    GL_RGB16I,
    GL_RGB32UI,
    GL_RGB32I,
    GL_RGBA8,
    GL_SRGB8_ALPHA8,
    GL_RGBA8_SNORM,
    GL_RGB10_A2,
    GL_RGBA16F,
    GL_RGBA32F,
    GL_RGBA8UI,
    GL_RGBA8I,
    GL_RGB10_A2UI,
    GL_RGBA16UI,
    GL_RGBA16I,
    GL_RGBA32UI,
    GL_RGBA32I,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

static const GLenum deprecated_texture_internal_format_storage_table_es3[] = {
    GL_ALPHA8_EXT,   GL_LUMINANCE8_EXT,   GL_LUMINANCE8_ALPHA8_EXT,
    GL_ALPHA16F_EXT, GL_LUMINANCE16F_EXT, GL_LUMINANCE_ALPHA16F_EXT,
    GL_ALPHA32F_EXT, GL_LUMINANCE32F_EXT, GL_LUMINANCE_ALPHA32F_EXT,
};

bool Validators::TextureMagFilterModeValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_NEAREST:
    case GL_LINEAR:
      return true;
  }
  return false;
}

bool Validators::TextureMinFilterModeValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_NEAREST:
    case GL_LINEAR:
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_LINEAR:
      return true;
  }
  return false;
}

static const GLenum valid_texture_parameter_table[] = {
    GL_TEXTURE_MAG_FILTER,
    GL_TEXTURE_MIN_FILTER,
    GL_TEXTURE_WRAP_S,
    GL_TEXTURE_WRAP_T,
};

static const GLenum valid_texture_parameter_table_es3[] = {
    GL_TEXTURE_BASE_LEVEL,       GL_TEXTURE_COMPARE_FUNC,
    GL_TEXTURE_COMPARE_MODE,     GL_TEXTURE_IMMUTABLE_FORMAT,
    GL_TEXTURE_IMMUTABLE_LEVELS, GL_TEXTURE_MAX_LEVEL,
    GL_TEXTURE_MAX_LOD,          GL_TEXTURE_MIN_LOD,
    GL_TEXTURE_WRAP_R,
};

static const GLenum
    valid_texture_sized_color_renderable_internal_format_table[] = {
        GL_R8,       GL_R8UI,     GL_R8I,          GL_R16UI,      GL_R16I,
        GL_R32UI,    GL_R32I,     GL_RG8,          GL_RG8UI,      GL_RG8I,
        GL_RG16UI,   GL_RG16I,    GL_RG32UI,       GL_RG32I,      GL_RGB8,
        GL_RGB565,   GL_RGBA8,    GL_SRGB8_ALPHA8, GL_RGB5_A1,    GL_RGBA4,
        GL_RGB10_A2, GL_RGBA8UI,  GL_RGBA8I,       GL_RGB10_A2UI, GL_RGBA16UI,
        GL_RGBA16I,  GL_RGBA32UI, GL_RGBA32I,
};

static const GLenum
    valid_texture_sized_texture_filterable_internal_format_table[] = {
        GL_R8,        GL_R8_SNORM,   GL_R16F,           GL_RG8,
        GL_RG8_SNORM, GL_RG16F,      GL_RGB8,           GL_SRGB8,
        GL_RGB565,    GL_RGB8_SNORM, GL_R11F_G11F_B10F, GL_RGB9_E5,
        GL_RGB16F,    GL_RGBA8,      GL_SRGB8_ALPHA8,   GL_RGBA8_SNORM,
        GL_RGB5_A1,   GL_RGBA4,      GL_RGB10_A2,       GL_RGBA16F,
        GL_R16_EXT,
};

bool Validators::TextureSrgbDecodeExtValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_DECODE_EXT:
    case GL_SKIP_DECODE_EXT:
      return true;
  }
  return false;
}

static const GLenum
    valid_texture_stencil_renderable_internal_format_table_es3[] = {
        GL_STENCIL_INDEX8,
        GL_DEPTH24_STENCIL8,
        GL_DEPTH32F_STENCIL8,
};

bool Validators::TextureSwizzleValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_ZERO:
    case GL_ONE:
      return true;
  }
  return false;
}

static const GLenum valid_texture_target_table[] = {
    GL_TEXTURE_2D,
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

static const GLenum valid_texture_unsized_internal_format_table[] = {
    GL_ALPHA, GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA,
};

bool Validators::TextureUsageValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_NONE:
    case GL_FRAMEBUFFER_ATTACHMENT_ANGLE:
      return true;
  }
  return false;
}

bool Validators::TextureWrapModeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_CLAMP_TO_EDGE:
    case GL_MIRRORED_REPEAT:
    case GL_REPEAT:
      return true;
  }
  return false;
}

static const GLenum valid_transform_feedback_bind_target_table[] = {
    GL_TRANSFORM_FEEDBACK,
};

bool Validators::TransformFeedbackPrimitiveModeValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_POINTS:
    case GL_LINES:
    case GL_TRIANGLES:
      return true;
  }
  return false;
}

bool Validators::UniformBlockParameterValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_UNIFORM_BLOCK_BINDING:
    case GL_UNIFORM_BLOCK_DATA_SIZE:
    case GL_UNIFORM_BLOCK_NAME_LENGTH:
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
    case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
    case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
      return true;
  }
  return false;
}

bool Validators::UniformParameterValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_UNIFORM_SIZE:
    case GL_UNIFORM_TYPE:
    case GL_UNIFORM_NAME_LENGTH:
    case GL_UNIFORM_BLOCK_INDEX:
    case GL_UNIFORM_OFFSET:
    case GL_UNIFORM_ARRAY_STRIDE:
    case GL_UNIFORM_MATRIX_STRIDE:
    case GL_UNIFORM_IS_ROW_MAJOR:
      return true;
  }
  return false;
}

bool Validators::VertexAttribITypeValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_INT:
    case GL_UNSIGNED_INT:
      return true;
  }
  return false;
}

static const GLenum valid_vertex_attrib_type_table[] = {
    GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_FLOAT,
};

static const GLenum valid_vertex_attrib_type_table_es3[] = {
    GL_INT,
    GL_UNSIGNED_INT,
    GL_HALF_FLOAT,
    GL_INT_2_10_10_10_REV,
    GL_UNSIGNED_INT_2_10_10_10_REV,
};

static const GLenum valid_vertex_attribute_table[] = {
    GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
    GL_VERTEX_ATTRIB_ARRAY_ENABLED,    GL_VERTEX_ATTRIB_ARRAY_SIZE,
    GL_VERTEX_ATTRIB_ARRAY_STRIDE,     GL_VERTEX_ATTRIB_ARRAY_TYPE,
    GL_CURRENT_VERTEX_ATTRIB,
};

static const GLenum valid_vertex_attribute_table_es3[] = {
    GL_VERTEX_ATTRIB_ARRAY_INTEGER,
    GL_VERTEX_ATTRIB_ARRAY_DIVISOR,
};

static const GLenum valid_vertex_pointer_table[] = {
    GL_VERTEX_ATTRIB_ARRAY_POINTER,
};

bool Validators::WindowRectanglesModeValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_INCLUSIVE_EXT:
    case GL_EXCLUSIVE_EXT:
      return true;
  }
  return false;
}

Validators::Validators()
    : attachment(valid_attachment_table, std::size(valid_attachment_table)),
      attachment_query(valid_attachment_query_table,
                       std::size(valid_attachment_query_table)),
      bufferfi(valid_bufferfi_table, std::size(valid_bufferfi_table)),
      bufferuiv(valid_bufferuiv_table, std::size(valid_bufferuiv_table)),
      capability(valid_capability_table, std::size(valid_capability_table)),
      compressed_texture_format(),
      dst_blend_factor(valid_dst_blend_factor_table,
                       std::size(valid_dst_blend_factor_table)),
      equation(valid_equation_table, std::size(valid_equation_table)),
      framebuffer_attachment_parameter(
          valid_framebuffer_attachment_parameter_table,
          std::size(valid_framebuffer_attachment_parameter_table)),
      framebuffer_parameter(),
      framebuffer_target(valid_framebuffer_target_table,
                         std::size(valid_framebuffer_target_table)),
      g_l_state(valid_g_l_state_table, std::size(valid_g_l_state_table)),
      get_tex_param_target(valid_get_tex_param_target_table,
                           std::size(valid_get_tex_param_target_table)),
      hint_target(valid_hint_target_table, std::size(valid_hint_target_table)),
      image_internal_format(valid_image_internal_format_table,
                            std::size(valid_image_internal_format_table)),
      index_type(valid_index_type_table, std::size(valid_index_type_table)),
      indexed_g_l_state(valid_indexed_g_l_state_table,
                        std::size(valid_indexed_g_l_state_table)),
      pixel_store(valid_pixel_store_table, std::size(valid_pixel_store_table)),
      pixel_type(valid_pixel_type_table, std::size(valid_pixel_type_table)),
      program_parameter(valid_program_parameter_table,
                        std::size(valid_program_parameter_table)),
      read_buffer(valid_read_buffer_table, std::size(valid_read_buffer_table)),
      read_pixel_format(valid_read_pixel_format_table,
                        std::size(valid_read_pixel_format_table)),
      read_pixel_type(valid_read_pixel_type_table,
                      std::size(valid_read_pixel_type_table)),
      render_buffer_format(valid_render_buffer_format_table,
                           std::size(valid_render_buffer_format_table)),
      render_buffer_parameter(valid_render_buffer_parameter_table,
                              std::size(valid_render_buffer_parameter_table)),
      render_buffer_target(valid_render_buffer_target_table,
                           std::size(valid_render_buffer_target_table)),
      sampler_parameter(valid_sampler_parameter_table,
                        std::size(valid_sampler_parameter_table)),
      shader_binary_format(),
      shader_parameter(valid_shader_parameter_table,
                       std::size(valid_shader_parameter_table)),
      src_blend_factor(valid_src_blend_factor_table,
                       std::size(valid_src_blend_factor_table)),
      sync_flush_flags(valid_sync_flush_flags_table,
                       std::size(valid_sync_flush_flags_table)),
      texture_bind_target(valid_texture_bind_target_table,
                          std::size(valid_texture_bind_target_table)),
      texture_compare_mode(valid_texture_compare_mode_table,
                           std::size(valid_texture_compare_mode_table)),
      texture_depth_renderable_internal_format(),
      texture_fbo_target(valid_texture_fbo_target_table,
                         std::size(valid_texture_fbo_target_table)),
      texture_format(valid_texture_format_table,
                     std::size(valid_texture_format_table)),
      texture_internal_format(valid_texture_internal_format_table,
                              std::size(valid_texture_internal_format_table)),
      texture_internal_format_storage(
          valid_texture_internal_format_storage_table,
          std::size(valid_texture_internal_format_storage_table)),
      texture_parameter(valid_texture_parameter_table,
                        std::size(valid_texture_parameter_table)),
      texture_sized_color_renderable_internal_format(
          valid_texture_sized_color_renderable_internal_format_table,
          std::size(
              valid_texture_sized_color_renderable_internal_format_table)),
      texture_sized_texture_filterable_internal_format(
          valid_texture_sized_texture_filterable_internal_format_table,
          std::size(
              valid_texture_sized_texture_filterable_internal_format_table)),
      texture_stencil_renderable_internal_format(),
      texture_target(valid_texture_target_table,
                     std::size(valid_texture_target_table)),
      texture_unsized_internal_format(
          valid_texture_unsized_internal_format_table,
          std::size(valid_texture_unsized_internal_format_table)),
      transform_feedback_bind_target(
          valid_transform_feedback_bind_target_table,
          std::size(valid_transform_feedback_bind_target_table)),
      vertex_attrib_type(valid_vertex_attrib_type_table,
                         std::size(valid_vertex_attrib_type_table)),
      vertex_attribute(valid_vertex_attribute_table,
                       std::size(valid_vertex_attribute_table)),
      vertex_pointer(valid_vertex_pointer_table,
                     std::size(valid_vertex_pointer_table)) {}

void Validators::UpdateValuesES3() {
  attachment.AddValues(valid_attachment_table_es3,
                       std::size(valid_attachment_table_es3));
  attachment_query.AddValues(valid_attachment_query_table_es3,
                             std::size(valid_attachment_query_table_es3));
  buffer_parameter.SetIsES3(true);
  buffer_target.SetIsES3(true);
  buffer_usage.SetIsES3(true);
  capability.AddValues(valid_capability_table_es3,
                       std::size(valid_capability_table_es3));
  dst_blend_factor.AddValues(valid_dst_blend_factor_table_es3,
                             std::size(valid_dst_blend_factor_table_es3));
  equation.AddValues(valid_equation_table_es3,
                     std::size(valid_equation_table_es3));
  framebuffer_attachment_parameter.AddValues(
      valid_framebuffer_attachment_parameter_table_es3,
      std::size(valid_framebuffer_attachment_parameter_table_es3));
  framebuffer_target.AddValues(valid_framebuffer_target_table_es3,
                               std::size(valid_framebuffer_target_table_es3));
  g_l_state.AddValues(valid_g_l_state_table_es3,
                      std::size(valid_g_l_state_table_es3));
  get_tex_param_target.AddValues(
      valid_get_tex_param_target_table_es3,
      std::size(valid_get_tex_param_target_table_es3));
  hint_target.AddValues(valid_hint_target_table_es3,
                        std::size(valid_hint_target_table_es3));
  index_type.AddValues(valid_index_type_table_es3,
                       std::size(valid_index_type_table_es3));
  pixel_store.AddValues(valid_pixel_store_table_es3,
                        std::size(valid_pixel_store_table_es3));
  pixel_type.AddValues(valid_pixel_type_table_es3,
                       std::size(valid_pixel_type_table_es3));
  program_parameter.AddValues(valid_program_parameter_table_es3,
                              std::size(valid_program_parameter_table_es3));
  read_pixel_format.AddValues(valid_read_pixel_format_table_es3,
                              std::size(valid_read_pixel_format_table_es3));
  read_pixel_type.AddValues(valid_read_pixel_type_table_es3,
                            std::size(valid_read_pixel_type_table_es3));
  render_buffer_format.AddValues(
      valid_render_buffer_format_table_es3,
      std::size(valid_render_buffer_format_table_es3));
  render_buffer_parameter.AddValues(
      valid_render_buffer_parameter_table_es3,
      std::size(valid_render_buffer_parameter_table_es3));
  texture_bind_target.AddValues(valid_texture_bind_target_table_es3,
                                std::size(valid_texture_bind_target_table_es3));
  texture_depth_renderable_internal_format.AddValues(
      valid_texture_depth_renderable_internal_format_table_es3,
      std::size(valid_texture_depth_renderable_internal_format_table_es3));
  texture_format.AddValues(valid_texture_format_table_es3,
                           std::size(valid_texture_format_table_es3));
  texture_internal_format.AddValues(
      valid_texture_internal_format_table_es3,
      std::size(valid_texture_internal_format_table_es3));
  texture_internal_format_storage.RemoveValues(
      deprecated_texture_internal_format_storage_table_es3,
      std::size(deprecated_texture_internal_format_storage_table_es3));
  texture_internal_format_storage.AddValues(
      valid_texture_internal_format_storage_table_es3,
      std::size(valid_texture_internal_format_storage_table_es3));
  texture_parameter.AddValues(valid_texture_parameter_table_es3,
                              std::size(valid_texture_parameter_table_es3));
  texture_stencil_renderable_internal_format.AddValues(
      valid_texture_stencil_renderable_internal_format_table_es3,
      std::size(valid_texture_stencil_renderable_internal_format_table_es3));
  vertex_attrib_type.AddValues(valid_vertex_attrib_type_table_es3,
                               std::size(valid_vertex_attrib_type_table_es3));
  vertex_attribute.AddValues(valid_vertex_attribute_table_es3,
                             std::size(valid_vertex_attribute_table_es3));
}

void Validators::UpdateETCCompressedTextureFormats() {
  compressed_texture_format.AddValue(GL_COMPRESSED_R11_EAC);
  compressed_texture_format.AddValue(GL_COMPRESSED_SIGNED_R11_EAC);
  compressed_texture_format.AddValue(GL_COMPRESSED_RG11_EAC);
  compressed_texture_format.AddValue(GL_COMPRESSED_SIGNED_RG11_EAC);
  compressed_texture_format.AddValue(GL_COMPRESSED_RGB8_ETC2);
  compressed_texture_format.AddValue(GL_COMPRESSED_SRGB8_ETC2);
  compressed_texture_format.AddValue(
      GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  compressed_texture_format.AddValue(
      GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  compressed_texture_format.AddValue(GL_COMPRESSED_RGBA8_ETC2_EAC);
  compressed_texture_format.AddValue(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_R11_EAC);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_SIGNED_R11_EAC);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_RG11_EAC);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_SIGNED_RG11_EAC);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_RGB8_ETC2);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_SRGB8_ETC2);
  texture_internal_format_storage.AddValue(
      GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  texture_internal_format_storage.AddValue(
      GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_RGBA8_ETC2_EAC);
  texture_internal_format_storage.AddValue(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
}

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
