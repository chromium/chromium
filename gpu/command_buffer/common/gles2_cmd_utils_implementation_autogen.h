// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_IMPLEMENTATION_AUTOGEN_H_

std::string GLES2Util::GetStringAttachment(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_COLOR_ATTACHMENT0, "GL_COLOR_ATTACHMENT0"},
      {GL_DEPTH_ATTACHMENT, "GL_DEPTH_ATTACHMENT"},
      {GL_STENCIL_ATTACHMENT, "GL_STENCIL_ATTACHMENT"},
      {GL_DEPTH_STENCIL_ATTACHMENT, "GL_DEPTH_STENCIL_ATTACHMENT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringAttachmentQuery(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_COLOR_ATTACHMENT0, "GL_COLOR_ATTACHMENT0"},
      {GL_DEPTH_ATTACHMENT, "GL_DEPTH_ATTACHMENT"},
      {GL_STENCIL_ATTACHMENT, "GL_STENCIL_ATTACHMENT"},
      {GL_DEPTH_STENCIL_ATTACHMENT, "GL_DEPTH_STENCIL_ATTACHMENT"},
      {GL_COLOR_EXT, "GL_COLOR_EXT"},
      {GL_DEPTH_EXT, "GL_DEPTH_EXT"},
      {GL_STENCIL_EXT, "GL_STENCIL_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBackbufferAttachment(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_COLOR_EXT, "GL_COLOR_EXT"},
      {GL_DEPTH_EXT, "GL_DEPTH_EXT"},
      {GL_STENCIL_EXT, "GL_STENCIL_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBlitFilter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NEAREST, "GL_NEAREST"},
      {GL_LINEAR, "GL_LINEAR"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_INTERLEAVED_ATTRIBS, "GL_INTERLEAVED_ATTRIBS"},
      {GL_SEPARATE_ATTRIBS, "GL_SEPARATE_ATTRIBS"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_BUFFER_SIZE, "GL_BUFFER_SIZE"},
      {GL_BUFFER_USAGE, "GL_BUFFER_USAGE"},
      {GL_BUFFER_ACCESS_FLAGS, "GL_BUFFER_ACCESS_FLAGS"},
      {GL_BUFFER_MAPPED, "GL_BUFFER_MAPPED"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferParameter64(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_BUFFER_SIZE, "GL_BUFFER_SIZE"},
      {GL_BUFFER_MAP_LENGTH, "GL_BUFFER_MAP_LENGTH"},
      {GL_BUFFER_MAP_OFFSET, "GL_BUFFER_MAP_OFFSET"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ARRAY_BUFFER, "GL_ARRAY_BUFFER"},
      {GL_ELEMENT_ARRAY_BUFFER, "GL_ELEMENT_ARRAY_BUFFER"},
      {GL_COPY_READ_BUFFER, "GL_COPY_READ_BUFFER"},
      {GL_COPY_WRITE_BUFFER, "GL_COPY_WRITE_BUFFER"},
      {GL_PIXEL_PACK_BUFFER, "GL_PIXEL_PACK_BUFFER"},
      {GL_PIXEL_UNPACK_BUFFER, "GL_PIXEL_UNPACK_BUFFER"},
      {GL_TRANSFORM_FEEDBACK_BUFFER, "GL_TRANSFORM_FEEDBACK_BUFFER"},
      {GL_UNIFORM_BUFFER, "GL_UNIFORM_BUFFER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferUsage(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_STREAM_DRAW, "GL_STREAM_DRAW"},
      {GL_STATIC_DRAW, "GL_STATIC_DRAW"},
      {GL_DYNAMIC_DRAW, "GL_DYNAMIC_DRAW"},
      {GL_STREAM_READ, "GL_STREAM_READ"},
      {GL_STREAM_COPY, "GL_STREAM_COPY"},
      {GL_STATIC_READ, "GL_STATIC_READ"},
      {GL_STATIC_COPY, "GL_STATIC_COPY"},
      {GL_DYNAMIC_READ, "GL_DYNAMIC_READ"},
      {GL_DYNAMIC_COPY, "GL_DYNAMIC_COPY"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferfi(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_DEPTH_STENCIL, "GL_DEPTH_STENCIL"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferfv(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_COLOR, "GL_COLOR"},
      {GL_DEPTH, "GL_DEPTH"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferiv(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_COLOR, "GL_COLOR"},
      {GL_STENCIL, "GL_STENCIL"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringBufferuiv(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_COLOR, "GL_COLOR"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringCapability(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_BLEND, "GL_BLEND"},
      {GL_CULL_FACE, "GL_CULL_FACE"},
      {GL_DEPTH_TEST, "GL_DEPTH_TEST"},
      {GL_DITHER, "GL_DITHER"},
      {GL_POLYGON_OFFSET_FILL, "GL_POLYGON_OFFSET_FILL"},
      {GL_SAMPLE_ALPHA_TO_COVERAGE, "GL_SAMPLE_ALPHA_TO_COVERAGE"},
      {GL_SAMPLE_COVERAGE, "GL_SAMPLE_COVERAGE"},
      {GL_SCISSOR_TEST, "GL_SCISSOR_TEST"},
      {GL_STENCIL_TEST, "GL_STENCIL_TEST"},
      {GL_RASTERIZER_DISCARD, "GL_RASTERIZER_DISCARD"},
      {GL_PRIMITIVE_RESTART_FIXED_INDEX, "GL_PRIMITIVE_RESTART_FIXED_INDEX"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringCmpFunction(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NEVER, "GL_NEVER"},     {GL_LESS, "GL_LESS"},
      {GL_EQUAL, "GL_EQUAL"},     {GL_LEQUAL, "GL_LEQUAL"},
      {GL_GREATER, "GL_GREATER"}, {GL_NOTEQUAL, "GL_NOTEQUAL"},
      {GL_GEQUAL, "GL_GEQUAL"},   {GL_ALWAYS, "GL_ALWAYS"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringCompressedTextureFormat(uint32_t value) {
  return GLES2Util::GetQualifiedEnumString(nullptr, 0, value);
}

std::string GLES2Util::GetStringDrawMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_POINTS, "GL_POINTS"},
      {GL_LINE_STRIP, "GL_LINE_STRIP"},
      {GL_LINE_LOOP, "GL_LINE_LOOP"},
      {GL_LINES, "GL_LINES"},
      {GL_TRIANGLE_STRIP, "GL_TRIANGLE_STRIP"},
      {GL_TRIANGLE_FAN, "GL_TRIANGLE_FAN"},
      {GL_TRIANGLES, "GL_TRIANGLES"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringDstBlendFactor(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ZERO, "GL_ZERO"},
      {GL_ONE, "GL_ONE"},
      {GL_SRC_COLOR, "GL_SRC_COLOR"},
      {GL_ONE_MINUS_SRC_COLOR, "GL_ONE_MINUS_SRC_COLOR"},
      {GL_DST_COLOR, "GL_DST_COLOR"},
      {GL_ONE_MINUS_DST_COLOR, "GL_ONE_MINUS_DST_COLOR"},
      {GL_SRC_ALPHA, "GL_SRC_ALPHA"},
      {GL_ONE_MINUS_SRC_ALPHA, "GL_ONE_MINUS_SRC_ALPHA"},
      {GL_DST_ALPHA, "GL_DST_ALPHA"},
      {GL_ONE_MINUS_DST_ALPHA, "GL_ONE_MINUS_DST_ALPHA"},
      {GL_CONSTANT_COLOR, "GL_CONSTANT_COLOR"},
      {GL_ONE_MINUS_CONSTANT_COLOR, "GL_ONE_MINUS_CONSTANT_COLOR"},
      {GL_CONSTANT_ALPHA, "GL_CONSTANT_ALPHA"},
      {GL_ONE_MINUS_CONSTANT_ALPHA, "GL_ONE_MINUS_CONSTANT_ALPHA"},
      {GL_SRC_ALPHA_SATURATE, "GL_SRC_ALPHA_SATURATE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringEquation(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_FUNC_ADD, "GL_FUNC_ADD"},
      {GL_FUNC_SUBTRACT, "GL_FUNC_SUBTRACT"},
      {GL_FUNC_REVERSE_SUBTRACT, "GL_FUNC_REVERSE_SUBTRACT"},
      {GL_MIN, "GL_MIN"},
      {GL_MAX, "GL_MAX"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringFaceMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_CW, "GL_CW"},
      {GL_CCW, "GL_CCW"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringFaceType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_FRONT, "GL_FRONT"},
      {GL_BACK, "GL_BACK"},
      {GL_FRONT_AND_BACK, "GL_FRONT_AND_BACK"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringFramebufferAttachmentParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
       "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE"},
      {GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
       "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME"},
      {GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
       "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL"},
      {GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
       "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE"},
      {GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,
       "GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE"},
      {GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
       "GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE"},
      {GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,
       "GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE"},
      {GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
       "GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE"},
      {GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
       "GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE"},
      {GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
       "GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE"},
      {GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,
       "GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"},
      {GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
       "GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING"},
      {GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
       "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringFramebufferParameter(uint32_t value) {
  return GLES2Util::GetQualifiedEnumString(nullptr, 0, value);
}

std::string GLES2Util::GetStringFramebufferTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_FRAMEBUFFER, "GL_FRAMEBUFFER"},
      {GL_DRAW_FRAMEBUFFER, "GL_DRAW_FRAMEBUFFER"},
      {GL_READ_FRAMEBUFFER, "GL_READ_FRAMEBUFFER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringGLState(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ACTIVE_TEXTURE, "GL_ACTIVE_TEXTURE"},
      {GL_ALIASED_LINE_WIDTH_RANGE, "GL_ALIASED_LINE_WIDTH_RANGE"},
      {GL_ALIASED_POINT_SIZE_RANGE, "GL_ALIASED_POINT_SIZE_RANGE"},
      {GL_ALPHA_BITS, "GL_ALPHA_BITS"},
      {GL_ARRAY_BUFFER_BINDING, "GL_ARRAY_BUFFER_BINDING"},
      {GL_BLUE_BITS, "GL_BLUE_BITS"},
      {GL_COMPRESSED_TEXTURE_FORMATS, "GL_COMPRESSED_TEXTURE_FORMATS"},
      {GL_CURRENT_PROGRAM, "GL_CURRENT_PROGRAM"},
      {GL_DEPTH_BITS, "GL_DEPTH_BITS"},
      {GL_DEPTH_RANGE, "GL_DEPTH_RANGE"},
      {GL_ELEMENT_ARRAY_BUFFER_BINDING, "GL_ELEMENT_ARRAY_BUFFER_BINDING"},
      {GL_FRAMEBUFFER_BINDING, "GL_FRAMEBUFFER_BINDING"},
      {GL_GENERATE_MIPMAP_HINT, "GL_GENERATE_MIPMAP_HINT"},
      {GL_GREEN_BITS, "GL_GREEN_BITS"},
      {GL_IMPLEMENTATION_COLOR_READ_FORMAT,
       "GL_IMPLEMENTATION_COLOR_READ_FORMAT"},
      {GL_IMPLEMENTATION_COLOR_READ_TYPE, "GL_IMPLEMENTATION_COLOR_READ_TYPE"},
      {GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
       "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS"},
      {GL_MAX_CUBE_MAP_TEXTURE_SIZE, "GL_MAX_CUBE_MAP_TEXTURE_SIZE"},
      {GL_MAX_FRAGMENT_UNIFORM_VECTORS, "GL_MAX_FRAGMENT_UNIFORM_VECTORS"},
      {GL_MAX_RENDERBUFFER_SIZE, "GL_MAX_RENDERBUFFER_SIZE"},
      {GL_MAX_TEXTURE_IMAGE_UNITS, "GL_MAX_TEXTURE_IMAGE_UNITS"},
      {GL_MAX_TEXTURE_SIZE, "GL_MAX_TEXTURE_SIZE"},
      {GL_MAX_VARYING_VECTORS, "GL_MAX_VARYING_VECTORS"},
      {GL_MAX_VERTEX_ATTRIBS, "GL_MAX_VERTEX_ATTRIBS"},
      {GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS"},
      {GL_MAX_VERTEX_UNIFORM_VECTORS, "GL_MAX_VERTEX_UNIFORM_VECTORS"},
      {GL_MAX_VIEWPORT_DIMS, "GL_MAX_VIEWPORT_DIMS"},
      {GL_NUM_COMPRESSED_TEXTURE_FORMATS, "GL_NUM_COMPRESSED_TEXTURE_FORMATS"},
      {GL_NUM_SHADER_BINARY_FORMATS, "GL_NUM_SHADER_BINARY_FORMATS"},
      {GL_PACK_ALIGNMENT, "GL_PACK_ALIGNMENT"},
      {GL_RED_BITS, "GL_RED_BITS"},
      {GL_RENDERBUFFER_BINDING, "GL_RENDERBUFFER_BINDING"},
      {GL_SAMPLE_BUFFERS, "GL_SAMPLE_BUFFERS"},
      {GL_SAMPLE_COVERAGE_INVERT, "GL_SAMPLE_COVERAGE_INVERT"},
      {GL_SAMPLE_COVERAGE_VALUE, "GL_SAMPLE_COVERAGE_VALUE"},
      {GL_SAMPLES, "GL_SAMPLES"},
      {GL_SCISSOR_BOX, "GL_SCISSOR_BOX"},
      {GL_SHADER_BINARY_FORMATS, "GL_SHADER_BINARY_FORMATS"},
      {GL_SHADER_COMPILER, "GL_SHADER_COMPILER"},
      {GL_SUBPIXEL_BITS, "GL_SUBPIXEL_BITS"},
      {GL_STENCIL_BITS, "GL_STENCIL_BITS"},
      {GL_TEXTURE_BINDING_2D, "GL_TEXTURE_BINDING_2D"},
      {GL_TEXTURE_BINDING_CUBE_MAP, "GL_TEXTURE_BINDING_CUBE_MAP"},
      {GL_UNPACK_ALIGNMENT, "GL_UNPACK_ALIGNMENT"},
      {GL_BIND_GENERATES_RESOURCE_CHROMIUM,
       "GL_BIND_GENERATES_RESOURCE_CHROMIUM"},
      {GL_VERTEX_ARRAY_BINDING_OES, "GL_VERTEX_ARRAY_BINDING_OES"},
      {GL_VIEWPORT, "GL_VIEWPORT"},
      {GL_BLEND_COLOR, "GL_BLEND_COLOR"},
      {GL_BLEND_EQUATION_RGB, "GL_BLEND_EQUATION_RGB"},
      {GL_BLEND_EQUATION_ALPHA, "GL_BLEND_EQUATION_ALPHA"},
      {GL_BLEND_SRC_RGB, "GL_BLEND_SRC_RGB"},
      {GL_BLEND_DST_RGB, "GL_BLEND_DST_RGB"},
      {GL_BLEND_SRC_ALPHA, "GL_BLEND_SRC_ALPHA"},
      {GL_BLEND_DST_ALPHA, "GL_BLEND_DST_ALPHA"},
      {GL_COLOR_CLEAR_VALUE, "GL_COLOR_CLEAR_VALUE"},
      {GL_DEPTH_CLEAR_VALUE, "GL_DEPTH_CLEAR_VALUE"},
      {GL_STENCIL_CLEAR_VALUE, "GL_STENCIL_CLEAR_VALUE"},
      {GL_COLOR_WRITEMASK, "GL_COLOR_WRITEMASK"},
      {GL_CULL_FACE_MODE, "GL_CULL_FACE_MODE"},
      {GL_DEPTH_FUNC, "GL_DEPTH_FUNC"},
      {GL_DEPTH_WRITEMASK, "GL_DEPTH_WRITEMASK"},
      {GL_FRONT_FACE, "GL_FRONT_FACE"},
      {GL_LINE_WIDTH, "GL_LINE_WIDTH"},
      {GL_POLYGON_OFFSET_FACTOR, "GL_POLYGON_OFFSET_FACTOR"},
      {GL_POLYGON_OFFSET_UNITS, "GL_POLYGON_OFFSET_UNITS"},
      {GL_STENCIL_FUNC, "GL_STENCIL_FUNC"},
      {GL_STENCIL_REF, "GL_STENCIL_REF"},
      {GL_STENCIL_VALUE_MASK, "GL_STENCIL_VALUE_MASK"},
      {GL_STENCIL_BACK_FUNC, "GL_STENCIL_BACK_FUNC"},
      {GL_STENCIL_BACK_REF, "GL_STENCIL_BACK_REF"},
      {GL_STENCIL_BACK_VALUE_MASK, "GL_STENCIL_BACK_VALUE_MASK"},
      {GL_STENCIL_WRITEMASK, "GL_STENCIL_WRITEMASK"},
      {GL_STENCIL_BACK_WRITEMASK, "GL_STENCIL_BACK_WRITEMASK"},
      {GL_STENCIL_FAIL, "GL_STENCIL_FAIL"},
      {GL_STENCIL_PASS_DEPTH_FAIL, "GL_STENCIL_PASS_DEPTH_FAIL"},
      {GL_STENCIL_PASS_DEPTH_PASS, "GL_STENCIL_PASS_DEPTH_PASS"},
      {GL_STENCIL_BACK_FAIL, "GL_STENCIL_BACK_FAIL"},
      {GL_STENCIL_BACK_PASS_DEPTH_FAIL, "GL_STENCIL_BACK_PASS_DEPTH_FAIL"},
      {GL_STENCIL_BACK_PASS_DEPTH_PASS, "GL_STENCIL_BACK_PASS_DEPTH_PASS"},
      {GL_BLEND, "GL_BLEND"},
      {GL_CULL_FACE, "GL_CULL_FACE"},
      {GL_DEPTH_TEST, "GL_DEPTH_TEST"},
      {GL_DITHER, "GL_DITHER"},
      {GL_POLYGON_OFFSET_FILL, "GL_POLYGON_OFFSET_FILL"},
      {GL_SAMPLE_ALPHA_TO_COVERAGE, "GL_SAMPLE_ALPHA_TO_COVERAGE"},
      {GL_SAMPLE_COVERAGE, "GL_SAMPLE_COVERAGE"},
      {GL_SCISSOR_TEST, "GL_SCISSOR_TEST"},
      {GL_STENCIL_TEST, "GL_STENCIL_TEST"},
      {GL_RASTERIZER_DISCARD, "GL_RASTERIZER_DISCARD"},
      {GL_PRIMITIVE_RESTART_FIXED_INDEX, "GL_PRIMITIVE_RESTART_FIXED_INDEX"},
      {GL_COPY_READ_BUFFER_BINDING, "GL_COPY_READ_BUFFER_BINDING"},
      {GL_COPY_WRITE_BUFFER_BINDING, "GL_COPY_WRITE_BUFFER_BINDING"},
      {GL_DRAW_BUFFER0, "GL_DRAW_BUFFER0"},
      {GL_DRAW_BUFFER1, "GL_DRAW_BUFFER1"},
      {GL_DRAW_BUFFER2, "GL_DRAW_BUFFER2"},
      {GL_DRAW_BUFFER3, "GL_DRAW_BUFFER3"},
      {GL_DRAW_BUFFER4, "GL_DRAW_BUFFER4"},
      {GL_DRAW_BUFFER5, "GL_DRAW_BUFFER5"},
      {GL_DRAW_BUFFER6, "GL_DRAW_BUFFER6"},
      {GL_DRAW_BUFFER7, "GL_DRAW_BUFFER7"},
      {GL_DRAW_BUFFER8, "GL_DRAW_BUFFER8"},
      {GL_DRAW_BUFFER9, "GL_DRAW_BUFFER9"},
      {GL_DRAW_BUFFER10, "GL_DRAW_BUFFER10"},
      {GL_DRAW_BUFFER11, "GL_DRAW_BUFFER11"},
      {GL_DRAW_BUFFER12, "GL_DRAW_BUFFER12"},
      {GL_DRAW_BUFFER13, "GL_DRAW_BUFFER13"},
      {GL_DRAW_BUFFER14, "GL_DRAW_BUFFER14"},
      {GL_DRAW_BUFFER15, "GL_DRAW_BUFFER15"},
      {GL_DRAW_FRAMEBUFFER_BINDING, "GL_DRAW_FRAMEBUFFER_BINDING"},
      {GL_FRAGMENT_SHADER_DERIVATIVE_HINT,
       "GL_FRAGMENT_SHADER_DERIVATIVE_HINT"},
      {GL_GPU_DISJOINT_EXT, "GL_GPU_DISJOINT_EXT"},
      {GL_MAJOR_VERSION, "GL_MAJOR_VERSION"},
      {GL_MAX_3D_TEXTURE_SIZE, "GL_MAX_3D_TEXTURE_SIZE"},
      {GL_MAX_ARRAY_TEXTURE_LAYERS, "GL_MAX_ARRAY_TEXTURE_LAYERS"},
      {GL_MAX_COLOR_ATTACHMENTS, "GL_MAX_COLOR_ATTACHMENTS"},
      {GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
       "GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS"},
      {GL_MAX_COMBINED_UNIFORM_BLOCKS, "GL_MAX_COMBINED_UNIFORM_BLOCKS"},
      {GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
       "GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS"},
      {GL_MAX_DRAW_BUFFERS, "GL_MAX_DRAW_BUFFERS"},
      {GL_MAX_ELEMENT_INDEX, "GL_MAX_ELEMENT_INDEX"},
      {GL_MAX_ELEMENTS_INDICES, "GL_MAX_ELEMENTS_INDICES"},
      {GL_MAX_ELEMENTS_VERTICES, "GL_MAX_ELEMENTS_VERTICES"},
      {GL_MAX_FRAGMENT_INPUT_COMPONENTS, "GL_MAX_FRAGMENT_INPUT_COMPONENTS"},
      {GL_MAX_FRAGMENT_UNIFORM_BLOCKS, "GL_MAX_FRAGMENT_UNIFORM_BLOCKS"},
      {GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
       "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS"},
      {GL_MAX_PROGRAM_TEXEL_OFFSET, "GL_MAX_PROGRAM_TEXEL_OFFSET"},
      {GL_MAX_SAMPLES, "GL_MAX_SAMPLES"},
      {GL_MAX_SERVER_WAIT_TIMEOUT, "GL_MAX_SERVER_WAIT_TIMEOUT"},
      {GL_MAX_TEXTURE_LOD_BIAS, "GL_MAX_TEXTURE_LOD_BIAS"},
      {GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
       "GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS"},
      {GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
       "GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS"},
      {GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
       "GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS"},
      {GL_MAX_UNIFORM_BLOCK_SIZE, "GL_MAX_UNIFORM_BLOCK_SIZE"},
      {GL_MAX_UNIFORM_BUFFER_BINDINGS, "GL_MAX_UNIFORM_BUFFER_BINDINGS"},
      {GL_MAX_VARYING_COMPONENTS, "GL_MAX_VARYING_COMPONENTS"},
      {GL_MAX_VERTEX_OUTPUT_COMPONENTS, "GL_MAX_VERTEX_OUTPUT_COMPONENTS"},
      {GL_MAX_VERTEX_UNIFORM_BLOCKS, "GL_MAX_VERTEX_UNIFORM_BLOCKS"},
      {GL_MAX_VERTEX_UNIFORM_COMPONENTS, "GL_MAX_VERTEX_UNIFORM_COMPONENTS"},
      {GL_MIN_PROGRAM_TEXEL_OFFSET, "GL_MIN_PROGRAM_TEXEL_OFFSET"},
      {GL_MINOR_VERSION, "GL_MINOR_VERSION"},
      {GL_NUM_EXTENSIONS, "GL_NUM_EXTENSIONS"},
      {GL_NUM_PROGRAM_BINARY_FORMATS, "GL_NUM_PROGRAM_BINARY_FORMATS"},
      {GL_PACK_ROW_LENGTH, "GL_PACK_ROW_LENGTH"},
      {GL_PACK_SKIP_PIXELS, "GL_PACK_SKIP_PIXELS"},
      {GL_PACK_SKIP_ROWS, "GL_PACK_SKIP_ROWS"},
      {GL_PIXEL_PACK_BUFFER_BINDING, "GL_PIXEL_PACK_BUFFER_BINDING"},
      {GL_PIXEL_UNPACK_BUFFER_BINDING, "GL_PIXEL_UNPACK_BUFFER_BINDING"},
      {GL_PROGRAM_BINARY_FORMATS, "GL_PROGRAM_BINARY_FORMATS"},
      {GL_READ_BUFFER, "GL_READ_BUFFER"},
      {GL_READ_FRAMEBUFFER_BINDING, "GL_READ_FRAMEBUFFER_BINDING"},
      {GL_SAMPLER_BINDING, "GL_SAMPLER_BINDING"},
      {GL_TIMESTAMP_EXT, "GL_TIMESTAMP_EXT"},
      {GL_TEXTURE_BINDING_2D_ARRAY, "GL_TEXTURE_BINDING_2D_ARRAY"},
      {GL_TEXTURE_BINDING_3D, "GL_TEXTURE_BINDING_3D"},
      {GL_TRANSFORM_FEEDBACK_BINDING, "GL_TRANSFORM_FEEDBACK_BINDING"},
      {GL_TRANSFORM_FEEDBACK_ACTIVE, "GL_TRANSFORM_FEEDBACK_ACTIVE"},
      {GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
       "GL_TRANSFORM_FEEDBACK_BUFFER_BINDING"},
      {GL_TRANSFORM_FEEDBACK_PAUSED, "GL_TRANSFORM_FEEDBACK_PAUSED"},
      {GL_UNIFORM_BUFFER_BINDING, "GL_UNIFORM_BUFFER_BINDING"},
      {GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
       "GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT"},
      {GL_UNPACK_IMAGE_HEIGHT, "GL_UNPACK_IMAGE_HEIGHT"},
      {GL_UNPACK_ROW_LENGTH, "GL_UNPACK_ROW_LENGTH"},
      {GL_UNPACK_SKIP_IMAGES, "GL_UNPACK_SKIP_IMAGES"},
      {GL_UNPACK_SKIP_PIXELS, "GL_UNPACK_SKIP_PIXELS"},
      {GL_UNPACK_SKIP_ROWS, "GL_UNPACK_SKIP_ROWS"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringGetMaxIndexType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE"},
      {GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT"},
      {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringGetTexParamTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_2D, "GL_TEXTURE_2D"},
      {GL_TEXTURE_CUBE_MAP, "GL_TEXTURE_CUBE_MAP"},
      {GL_TEXTURE_2D_ARRAY, "GL_TEXTURE_2D_ARRAY"},
      {GL_TEXTURE_3D, "GL_TEXTURE_3D"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringHintMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_FASTEST, "GL_FASTEST"},
      {GL_NICEST, "GL_NICEST"},
      {GL_DONT_CARE, "GL_DONT_CARE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringHintTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_GENERATE_MIPMAP_HINT, "GL_GENERATE_MIPMAP_HINT"},
      {GL_FRAGMENT_SHADER_DERIVATIVE_HINT,
       "GL_FRAGMENT_SHADER_DERIVATIVE_HINT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringImageInternalFormat(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_RGB, "GL_RGB"},
      {GL_RGBA, "GL_RGBA"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringIndexType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE"},
      {GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT"},
      {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringIndexedBufferTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TRANSFORM_FEEDBACK_BUFFER, "GL_TRANSFORM_FEEDBACK_BUFFER"},
      {GL_UNIFORM_BUFFER, "GL_UNIFORM_BUFFER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringIndexedGLState(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
       "GL_TRANSFORM_FEEDBACK_BUFFER_BINDING"},
      {GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, "GL_TRANSFORM_FEEDBACK_BUFFER_SIZE"},
      {GL_TRANSFORM_FEEDBACK_BUFFER_START,
       "GL_TRANSFORM_FEEDBACK_BUFFER_START"},
      {GL_UNIFORM_BUFFER_BINDING, "GL_UNIFORM_BUFFER_BINDING"},
      {GL_UNIFORM_BUFFER_SIZE, "GL_UNIFORM_BUFFER_SIZE"},
      {GL_UNIFORM_BUFFER_START, "GL_UNIFORM_BUFFER_START"},
      {GL_BLEND_EQUATION_RGB, "GL_BLEND_EQUATION_RGB"},
      {GL_BLEND_EQUATION_ALPHA, "GL_BLEND_EQUATION_ALPHA"},
      {GL_BLEND_SRC_RGB, "GL_BLEND_SRC_RGB"},
      {GL_BLEND_SRC_ALPHA, "GL_BLEND_SRC_ALPHA"},
      {GL_BLEND_DST_RGB, "GL_BLEND_DST_RGB"},
      {GL_BLEND_DST_ALPHA, "GL_BLEND_DST_ALPHA"},
      {GL_COLOR_WRITEMASK, "GL_COLOR_WRITEMASK"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringIndexedStringType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_EXTENSIONS, "GL_EXTENSIONS"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringInternalFormatParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NUM_SAMPLE_COUNTS, "GL_NUM_SAMPLE_COUNTS"},
      {GL_SAMPLES, "GL_SAMPLES"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringMapBufferAccess(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_MAP_READ_BIT, "GL_MAP_READ_BIT"},
      {GL_MAP_WRITE_BIT, "GL_MAP_WRITE_BIT"},
      {GL_MAP_INVALIDATE_RANGE_BIT, "GL_MAP_INVALIDATE_RANGE_BIT"},
      {GL_MAP_INVALIDATE_BUFFER_BIT, "GL_MAP_INVALIDATE_BUFFER_BIT"},
      {GL_MAP_FLUSH_EXPLICIT_BIT, "GL_MAP_FLUSH_EXPLICIT_BIT"},
      {GL_MAP_UNSYNCHRONIZED_BIT, "GL_MAP_UNSYNCHRONIZED_BIT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringPixelStore(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_PACK_ALIGNMENT, "GL_PACK_ALIGNMENT"},
      {GL_UNPACK_ALIGNMENT, "GL_UNPACK_ALIGNMENT"},
      {GL_PACK_ROW_LENGTH, "GL_PACK_ROW_LENGTH"},
      {GL_PACK_SKIP_PIXELS, "GL_PACK_SKIP_PIXELS"},
      {GL_PACK_SKIP_ROWS, "GL_PACK_SKIP_ROWS"},
      {GL_UNPACK_ROW_LENGTH, "GL_UNPACK_ROW_LENGTH"},
      {GL_UNPACK_IMAGE_HEIGHT, "GL_UNPACK_IMAGE_HEIGHT"},
      {GL_UNPACK_SKIP_PIXELS, "GL_UNPACK_SKIP_PIXELS"},
      {GL_UNPACK_SKIP_ROWS, "GL_UNPACK_SKIP_ROWS"},
      {GL_UNPACK_SKIP_IMAGES, "GL_UNPACK_SKIP_IMAGES"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringPixelType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE"},
      {GL_UNSIGNED_SHORT_5_6_5, "GL_UNSIGNED_SHORT_5_6_5"},
      {GL_UNSIGNED_SHORT_4_4_4_4, "GL_UNSIGNED_SHORT_4_4_4_4"},
      {GL_UNSIGNED_SHORT_5_5_5_1, "GL_UNSIGNED_SHORT_5_5_5_1"},
      {GL_BYTE, "GL_BYTE"},
      {GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT"},
      {GL_SHORT, "GL_SHORT"},
      {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
      {GL_INT, "GL_INT"},
      {GL_HALF_FLOAT, "GL_HALF_FLOAT"},
      {GL_FLOAT, "GL_FLOAT"},
      {GL_UNSIGNED_INT_2_10_10_10_REV, "GL_UNSIGNED_INT_2_10_10_10_REV"},
      {GL_UNSIGNED_INT_10F_11F_11F_REV, "GL_UNSIGNED_INT_10F_11F_11F_REV"},
      {GL_UNSIGNED_INT_5_9_9_9_REV, "GL_UNSIGNED_INT_5_9_9_9_REV"},
      {GL_UNSIGNED_INT_24_8, "GL_UNSIGNED_INT_24_8"},
      {GL_FLOAT_32_UNSIGNED_INT_24_8_REV, "GL_FLOAT_32_UNSIGNED_INT_24_8_REV"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringProgramParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_DELETE_STATUS, "GL_DELETE_STATUS"},
      {GL_LINK_STATUS, "GL_LINK_STATUS"},
      {GL_VALIDATE_STATUS, "GL_VALIDATE_STATUS"},
      {GL_INFO_LOG_LENGTH, "GL_INFO_LOG_LENGTH"},
      {GL_ATTACHED_SHADERS, "GL_ATTACHED_SHADERS"},
      {GL_ACTIVE_ATTRIBUTES, "GL_ACTIVE_ATTRIBUTES"},
      {GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH"},
      {GL_ACTIVE_UNIFORMS, "GL_ACTIVE_UNIFORMS"},
      {GL_ACTIVE_UNIFORM_MAX_LENGTH, "GL_ACTIVE_UNIFORM_MAX_LENGTH"},
      {GL_ACTIVE_UNIFORM_BLOCKS, "GL_ACTIVE_UNIFORM_BLOCKS"},
      {GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
       "GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH"},
      {GL_TRANSFORM_FEEDBACK_BUFFER_MODE, "GL_TRANSFORM_FEEDBACK_BUFFER_MODE"},
      {GL_TRANSFORM_FEEDBACK_VARYINGS, "GL_TRANSFORM_FEEDBACK_VARYINGS"},
      {GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
       "GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringQueryObjectParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_QUERY_RESULT_EXT, "GL_QUERY_RESULT_EXT"},
      {GL_QUERY_RESULT_AVAILABLE_EXT, "GL_QUERY_RESULT_AVAILABLE_EXT"},
      {GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT,
       "GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringQueryParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_CURRENT_QUERY_EXT, "GL_CURRENT_QUERY_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringQueryTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_SAMPLES_PASSED_ARB, "GL_SAMPLES_PASSED_ARB"},
      {GL_ANY_SAMPLES_PASSED_EXT, "GL_ANY_SAMPLES_PASSED_EXT"},
      {GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT,
       "GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT"},
      {GL_COMMANDS_ISSUED_CHROMIUM, "GL_COMMANDS_ISSUED_CHROMIUM"},
      {GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM,
       "GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM"},
      {GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM,
       "GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM"},
      {GL_COMMANDS_COMPLETED_CHROMIUM, "GL_COMMANDS_COMPLETED_CHROMIUM"},
      {GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM,
       "GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM"},
      {GL_PROGRAM_COMPLETION_QUERY_CHROMIUM,
       "GL_PROGRAM_COMPLETION_QUERY_CHROMIUM"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringReadBuffer(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NONE, "GL_NONE"},
      {GL_BACK, "GL_BACK"},
      {GL_COLOR_ATTACHMENT0, "GL_COLOR_ATTACHMENT0"},
      {GL_COLOR_ATTACHMENT1, "GL_COLOR_ATTACHMENT1"},
      {GL_COLOR_ATTACHMENT2, "GL_COLOR_ATTACHMENT2"},
      {GL_COLOR_ATTACHMENT3, "GL_COLOR_ATTACHMENT3"},
      {GL_COLOR_ATTACHMENT4, "GL_COLOR_ATTACHMENT4"},
      {GL_COLOR_ATTACHMENT5, "GL_COLOR_ATTACHMENT5"},
      {GL_COLOR_ATTACHMENT6, "GL_COLOR_ATTACHMENT6"},
      {GL_COLOR_ATTACHMENT7, "GL_COLOR_ATTACHMENT7"},
      {GL_COLOR_ATTACHMENT8, "GL_COLOR_ATTACHMENT8"},
      {GL_COLOR_ATTACHMENT9, "GL_COLOR_ATTACHMENT9"},
      {GL_COLOR_ATTACHMENT10, "GL_COLOR_ATTACHMENT10"},
      {GL_COLOR_ATTACHMENT11, "GL_COLOR_ATTACHMENT11"},
      {GL_COLOR_ATTACHMENT12, "GL_COLOR_ATTACHMENT12"},
      {GL_COLOR_ATTACHMENT13, "GL_COLOR_ATTACHMENT13"},
      {GL_COLOR_ATTACHMENT14, "GL_COLOR_ATTACHMENT14"},
      {GL_COLOR_ATTACHMENT15, "GL_COLOR_ATTACHMENT15"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringReadPixelFormat(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ALPHA, "GL_ALPHA"},
      {GL_RGB, "GL_RGB"},
      {GL_RGBA, "GL_RGBA"},
      {GL_RED, "GL_RED"},
      {GL_RED_INTEGER, "GL_RED_INTEGER"},
      {GL_RG, "GL_RG"},
      {GL_RG_INTEGER, "GL_RG_INTEGER"},
      {GL_RGB_INTEGER, "GL_RGB_INTEGER"},
      {GL_RGBA_INTEGER, "GL_RGBA_INTEGER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringReadPixelType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE"},
      {GL_UNSIGNED_SHORT_5_6_5, "GL_UNSIGNED_SHORT_5_6_5"},
      {GL_UNSIGNED_SHORT_4_4_4_4, "GL_UNSIGNED_SHORT_4_4_4_4"},
      {GL_UNSIGNED_SHORT_5_5_5_1, "GL_UNSIGNED_SHORT_5_5_5_1"},
      {GL_BYTE, "GL_BYTE"},
      {GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT"},
      {GL_SHORT, "GL_SHORT"},
      {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
      {GL_INT, "GL_INT"},
      {GL_HALF_FLOAT, "GL_HALF_FLOAT"},
      {GL_FLOAT, "GL_FLOAT"},
      {GL_UNSIGNED_INT_2_10_10_10_REV, "GL_UNSIGNED_INT_2_10_10_10_REV"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringRenderBufferFormat(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_RGBA4, "GL_RGBA4"},
      {GL_RGB565, "GL_RGB565"},
      {GL_RGB5_A1, "GL_RGB5_A1"},
      {GL_DEPTH_COMPONENT16, "GL_DEPTH_COMPONENT16"},
      {GL_STENCIL_INDEX8, "GL_STENCIL_INDEX8"},
      {GL_R8, "GL_R8"},
      {GL_R8UI, "GL_R8UI"},
      {GL_R8I, "GL_R8I"},
      {GL_R16UI, "GL_R16UI"},
      {GL_R16I, "GL_R16I"},
      {GL_R32UI, "GL_R32UI"},
      {GL_R32I, "GL_R32I"},
      {GL_RG8, "GL_RG8"},
      {GL_RG8UI, "GL_RG8UI"},
      {GL_RG8I, "GL_RG8I"},
      {GL_RG16UI, "GL_RG16UI"},
      {GL_RG16I, "GL_RG16I"},
      {GL_RG32UI, "GL_RG32UI"},
      {GL_RG32I, "GL_RG32I"},
      {GL_RGB8, "GL_RGB8"},
      {GL_RGBA8, "GL_RGBA8"},
      {GL_SRGB8_ALPHA8, "GL_SRGB8_ALPHA8"},
      {GL_RGB10_A2, "GL_RGB10_A2"},
      {GL_RGBA8UI, "GL_RGBA8UI"},
      {GL_RGBA8I, "GL_RGBA8I"},
      {GL_RGB10_A2UI, "GL_RGB10_A2UI"},
      {GL_RGBA16UI, "GL_RGBA16UI"},
      {GL_RGBA16I, "GL_RGBA16I"},
      {GL_RGBA32UI, "GL_RGBA32UI"},
      {GL_RGBA32I, "GL_RGBA32I"},
      {GL_DEPTH_COMPONENT24, "GL_DEPTH_COMPONENT24"},
      {GL_DEPTH_COMPONENT32F, "GL_DEPTH_COMPONENT32F"},
      {GL_DEPTH24_STENCIL8, "GL_DEPTH24_STENCIL8"},
      {GL_DEPTH32F_STENCIL8, "GL_DEPTH32F_STENCIL8"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringRenderBufferParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_RENDERBUFFER_RED_SIZE, "GL_RENDERBUFFER_RED_SIZE"},
      {GL_RENDERBUFFER_GREEN_SIZE, "GL_RENDERBUFFER_GREEN_SIZE"},
      {GL_RENDERBUFFER_BLUE_SIZE, "GL_RENDERBUFFER_BLUE_SIZE"},
      {GL_RENDERBUFFER_ALPHA_SIZE, "GL_RENDERBUFFER_ALPHA_SIZE"},
      {GL_RENDERBUFFER_DEPTH_SIZE, "GL_RENDERBUFFER_DEPTH_SIZE"},
      {GL_RENDERBUFFER_STENCIL_SIZE, "GL_RENDERBUFFER_STENCIL_SIZE"},
      {GL_RENDERBUFFER_WIDTH, "GL_RENDERBUFFER_WIDTH"},
      {GL_RENDERBUFFER_HEIGHT, "GL_RENDERBUFFER_HEIGHT"},
      {GL_RENDERBUFFER_INTERNAL_FORMAT, "GL_RENDERBUFFER_INTERNAL_FORMAT"},
      {GL_RENDERBUFFER_SAMPLES, "GL_RENDERBUFFER_SAMPLES"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringRenderBufferTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_RENDERBUFFER, "GL_RENDERBUFFER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringResetStatus(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_GUILTY_CONTEXT_RESET_ARB, "GL_GUILTY_CONTEXT_RESET_ARB"},
      {GL_INNOCENT_CONTEXT_RESET_ARB, "GL_INNOCENT_CONTEXT_RESET_ARB"},
      {GL_UNKNOWN_CONTEXT_RESET_ARB, "GL_UNKNOWN_CONTEXT_RESET_ARB"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringSamplerParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_MAG_FILTER, "GL_TEXTURE_MAG_FILTER"},
      {GL_TEXTURE_MIN_FILTER, "GL_TEXTURE_MIN_FILTER"},
      {GL_TEXTURE_MIN_LOD, "GL_TEXTURE_MIN_LOD"},
      {GL_TEXTURE_MAX_LOD, "GL_TEXTURE_MAX_LOD"},
      {GL_TEXTURE_WRAP_S, "GL_TEXTURE_WRAP_S"},
      {GL_TEXTURE_WRAP_T, "GL_TEXTURE_WRAP_T"},
      {GL_TEXTURE_WRAP_R, "GL_TEXTURE_WRAP_R"},
      {GL_TEXTURE_COMPARE_MODE, "GL_TEXTURE_COMPARE_MODE"},
      {GL_TEXTURE_COMPARE_FUNC, "GL_TEXTURE_COMPARE_FUNC"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringShaderBinaryFormat(uint32_t value) {
  return GLES2Util::GetQualifiedEnumString(nullptr, 0, value);
}

std::string GLES2Util::GetStringShaderParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_SHADER_TYPE, "GL_SHADER_TYPE"},
      {GL_DELETE_STATUS, "GL_DELETE_STATUS"},
      {GL_COMPILE_STATUS, "GL_COMPILE_STATUS"},
      {GL_INFO_LOG_LENGTH, "GL_INFO_LOG_LENGTH"},
      {GL_SHADER_SOURCE_LENGTH, "GL_SHADER_SOURCE_LENGTH"},
      {GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
       "GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringShaderPrecision(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_LOW_FLOAT, "GL_LOW_FLOAT"},   {GL_MEDIUM_FLOAT, "GL_MEDIUM_FLOAT"},
      {GL_HIGH_FLOAT, "GL_HIGH_FLOAT"}, {GL_LOW_INT, "GL_LOW_INT"},
      {GL_MEDIUM_INT, "GL_MEDIUM_INT"}, {GL_HIGH_INT, "GL_HIGH_INT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringShaderType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_VERTEX_SHADER, "GL_VERTEX_SHADER"},
      {GL_FRAGMENT_SHADER, "GL_FRAGMENT_SHADER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringSharedImageAccessMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
       "GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM"},
      {GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
       "GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringSrcBlendFactor(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ZERO, "GL_ZERO"},
      {GL_ONE, "GL_ONE"},
      {GL_SRC_COLOR, "GL_SRC_COLOR"},
      {GL_ONE_MINUS_SRC_COLOR, "GL_ONE_MINUS_SRC_COLOR"},
      {GL_DST_COLOR, "GL_DST_COLOR"},
      {GL_ONE_MINUS_DST_COLOR, "GL_ONE_MINUS_DST_COLOR"},
      {GL_SRC_ALPHA, "GL_SRC_ALPHA"},
      {GL_ONE_MINUS_SRC_ALPHA, "GL_ONE_MINUS_SRC_ALPHA"},
      {GL_DST_ALPHA, "GL_DST_ALPHA"},
      {GL_ONE_MINUS_DST_ALPHA, "GL_ONE_MINUS_DST_ALPHA"},
      {GL_CONSTANT_COLOR, "GL_CONSTANT_COLOR"},
      {GL_ONE_MINUS_CONSTANT_COLOR, "GL_ONE_MINUS_CONSTANT_COLOR"},
      {GL_CONSTANT_ALPHA, "GL_CONSTANT_ALPHA"},
      {GL_ONE_MINUS_CONSTANT_ALPHA, "GL_ONE_MINUS_CONSTANT_ALPHA"},
      {GL_SRC_ALPHA_SATURATE, "GL_SRC_ALPHA_SATURATE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringStencilOp(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_KEEP, "GL_KEEP"},           {GL_ZERO, "GL_ZERO"},
      {GL_REPLACE, "GL_REPLACE"},     {GL_INCR, "GL_INCR"},
      {GL_INCR_WRAP, "GL_INCR_WRAP"}, {GL_DECR, "GL_DECR"},
      {GL_DECR_WRAP, "GL_DECR_WRAP"}, {GL_INVERT, "GL_INVERT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringStringType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_VENDOR, "GL_VENDOR"},
      {GL_RENDERER, "GL_RENDERER"},
      {GL_VERSION, "GL_VERSION"},
      {GL_SHADING_LANGUAGE_VERSION, "GL_SHADING_LANGUAGE_VERSION"},
      {GL_EXTENSIONS, "GL_EXTENSIONS"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringSyncCondition(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_SYNC_GPU_COMMANDS_COMPLETE, "GL_SYNC_GPU_COMMANDS_COMPLETE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringSyncParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_SYNC_STATUS, "GL_SYNC_STATUS"},
      {GL_OBJECT_TYPE, "GL_OBJECT_TYPE"},
      {GL_SYNC_CONDITION, "GL_SYNC_CONDITION"},
      {GL_SYNC_FLAGS, "GL_SYNC_FLAGS"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTexture3DTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_3D, "GL_TEXTURE_3D"},
      {GL_TEXTURE_2D_ARRAY, "GL_TEXTURE_2D_ARRAY"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureBindTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_2D, "GL_TEXTURE_2D"},
      {GL_TEXTURE_CUBE_MAP, "GL_TEXTURE_CUBE_MAP"},
      {GL_TEXTURE_3D, "GL_TEXTURE_3D"},
      {GL_TEXTURE_2D_ARRAY, "GL_TEXTURE_2D_ARRAY"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureCompareFunc(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_LEQUAL, "GL_LEQUAL"}, {GL_GEQUAL, "GL_GEQUAL"},
      {GL_LESS, "GL_LESS"},     {GL_GREATER, "GL_GREATER"},
      {GL_EQUAL, "GL_EQUAL"},   {GL_NOTEQUAL, "GL_NOTEQUAL"},
      {GL_ALWAYS, "GL_ALWAYS"}, {GL_NEVER, "GL_NEVER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureCompareMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NONE, "GL_NONE"},
      {GL_COMPARE_REF_TO_TEXTURE, "GL_COMPARE_REF_TO_TEXTURE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureDepthRenderableInternalFormat(
    uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_DEPTH_COMPONENT16, "GL_DEPTH_COMPONENT16"},
      {GL_DEPTH_COMPONENT24, "GL_DEPTH_COMPONENT24"},
      {GL_DEPTH_COMPONENT32F, "GL_DEPTH_COMPONENT32F"},
      {GL_DEPTH24_STENCIL8, "GL_DEPTH24_STENCIL8"},
      {GL_DEPTH32F_STENCIL8, "GL_DEPTH32F_STENCIL8"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureFboTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_2D, "GL_TEXTURE_2D"},
      {GL_TEXTURE_CUBE_MAP_POSITIVE_X, "GL_TEXTURE_CUBE_MAP_POSITIVE_X"},
      {GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "GL_TEXTURE_CUBE_MAP_NEGATIVE_X"},
      {GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "GL_TEXTURE_CUBE_MAP_POSITIVE_Y"},
      {GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y"},
      {GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "GL_TEXTURE_CUBE_MAP_POSITIVE_Z"},
      {GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureFormat(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ALPHA, "GL_ALPHA"},
      {GL_LUMINANCE, "GL_LUMINANCE"},
      {GL_LUMINANCE_ALPHA, "GL_LUMINANCE_ALPHA"},
      {GL_RGB, "GL_RGB"},
      {GL_RGBA, "GL_RGBA"},
      {GL_RED, "GL_RED"},
      {GL_RED_INTEGER, "GL_RED_INTEGER"},
      {GL_RG, "GL_RG"},
      {GL_RG_INTEGER, "GL_RG_INTEGER"},
      {GL_RGB_INTEGER, "GL_RGB_INTEGER"},
      {GL_RGBA_INTEGER, "GL_RGBA_INTEGER"},
      {GL_DEPTH_COMPONENT, "GL_DEPTH_COMPONENT"},
      {GL_DEPTH_STENCIL, "GL_DEPTH_STENCIL"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureInternalFormat(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ALPHA, "GL_ALPHA"},
      {GL_LUMINANCE, "GL_LUMINANCE"},
      {GL_LUMINANCE_ALPHA, "GL_LUMINANCE_ALPHA"},
      {GL_RGB, "GL_RGB"},
      {GL_RGBA, "GL_RGBA"},
      {GL_R8, "GL_R8"},
      {GL_R8_SNORM, "GL_R8_SNORM"},
      {GL_R16F, "GL_R16F"},
      {GL_R32F, "GL_R32F"},
      {GL_R8UI, "GL_R8UI"},
      {GL_R8I, "GL_R8I"},
      {GL_R16UI, "GL_R16UI"},
      {GL_R16I, "GL_R16I"},
      {GL_R32UI, "GL_R32UI"},
      {GL_R32I, "GL_R32I"},
      {GL_RG8, "GL_RG8"},
      {GL_RG8_SNORM, "GL_RG8_SNORM"},
      {GL_RG16F, "GL_RG16F"},
      {GL_RG32F, "GL_RG32F"},
      {GL_RG8UI, "GL_RG8UI"},
      {GL_RG8I, "GL_RG8I"},
      {GL_RG16UI, "GL_RG16UI"},
      {GL_RG16I, "GL_RG16I"},
      {GL_RG32UI, "GL_RG32UI"},
      {GL_RG32I, "GL_RG32I"},
      {GL_RGB8, "GL_RGB8"},
      {GL_SRGB8, "GL_SRGB8"},
      {GL_RGB565, "GL_RGB565"},
      {GL_RGB8_SNORM, "GL_RGB8_SNORM"},
      {GL_R11F_G11F_B10F, "GL_R11F_G11F_B10F"},
      {GL_RGB9_E5, "GL_RGB9_E5"},
      {GL_RGB16F, "GL_RGB16F"},
      {GL_RGB32F, "GL_RGB32F"},
      {GL_RGB8UI, "GL_RGB8UI"},
      {GL_RGB8I, "GL_RGB8I"},
      {GL_RGB16UI, "GL_RGB16UI"},
      {GL_RGB16I, "GL_RGB16I"},
      {GL_RGB32UI, "GL_RGB32UI"},
      {GL_RGB32I, "GL_RGB32I"},
      {GL_RGBA8, "GL_RGBA8"},
      {GL_SRGB8_ALPHA8, "GL_SRGB8_ALPHA8"},
      {GL_RGBA8_SNORM, "GL_RGBA8_SNORM"},
      {GL_RGB5_A1, "GL_RGB5_A1"},
      {GL_RGBA4, "GL_RGBA4"},
      {GL_RGB10_A2, "GL_RGB10_A2"},
      {GL_RGBA16F, "GL_RGBA16F"},
      {GL_RGBA32F, "GL_RGBA32F"},
      {GL_RGBA8UI, "GL_RGBA8UI"},
      {GL_RGBA8I, "GL_RGBA8I"},
      {GL_RGB10_A2UI, "GL_RGB10_A2UI"},
      {GL_RGBA16UI, "GL_RGBA16UI"},
      {GL_RGBA16I, "GL_RGBA16I"},
      {GL_RGBA32UI, "GL_RGBA32UI"},
      {GL_RGBA32I, "GL_RGBA32I"},
      {GL_DEPTH_COMPONENT16, "GL_DEPTH_COMPONENT16"},
      {GL_DEPTH_COMPONENT24, "GL_DEPTH_COMPONENT24"},
      {GL_DEPTH_COMPONENT32F, "GL_DEPTH_COMPONENT32F"},
      {GL_DEPTH24_STENCIL8, "GL_DEPTH24_STENCIL8"},
      {GL_DEPTH32F_STENCIL8, "GL_DEPTH32F_STENCIL8"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureInternalFormatStorage(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_RGB565, "GL_RGB565"},
      {GL_RGBA4, "GL_RGBA4"},
      {GL_RGB5_A1, "GL_RGB5_A1"},
      {GL_ALPHA8_EXT, "GL_ALPHA8_EXT"},
      {GL_LUMINANCE8_EXT, "GL_LUMINANCE8_EXT"},
      {GL_LUMINANCE8_ALPHA8_EXT, "GL_LUMINANCE8_ALPHA8_EXT"},
      {GL_RGB8_OES, "GL_RGB8_OES"},
      {GL_RGBA8_OES, "GL_RGBA8_OES"},
      {GL_R8, "GL_R8"},
      {GL_R8_SNORM, "GL_R8_SNORM"},
      {GL_R16F, "GL_R16F"},
      {GL_R32F, "GL_R32F"},
      {GL_R8UI, "GL_R8UI"},
      {GL_R8I, "GL_R8I"},
      {GL_R16UI, "GL_R16UI"},
      {GL_R16I, "GL_R16I"},
      {GL_R32UI, "GL_R32UI"},
      {GL_R32I, "GL_R32I"},
      {GL_RG8, "GL_RG8"},
      {GL_RG8_SNORM, "GL_RG8_SNORM"},
      {GL_RG16F, "GL_RG16F"},
      {GL_RG32F, "GL_RG32F"},
      {GL_RG8UI, "GL_RG8UI"},
      {GL_RG8I, "GL_RG8I"},
      {GL_RG16UI, "GL_RG16UI"},
      {GL_RG16I, "GL_RG16I"},
      {GL_RG32UI, "GL_RG32UI"},
      {GL_RG32I, "GL_RG32I"},
      {GL_RGB8, "GL_RGB8"},
      {GL_SRGB8, "GL_SRGB8"},
      {GL_RGB8_SNORM, "GL_RGB8_SNORM"},
      {GL_R11F_G11F_B10F, "GL_R11F_G11F_B10F"},
      {GL_RGB9_E5, "GL_RGB9_E5"},
      {GL_RGB16F, "GL_RGB16F"},
      {GL_RGB32F, "GL_RGB32F"},
      {GL_RGB8UI, "GL_RGB8UI"},
      {GL_RGB8I, "GL_RGB8I"},
      {GL_RGB16UI, "GL_RGB16UI"},
      {GL_RGB16I, "GL_RGB16I"},
      {GL_RGB32UI, "GL_RGB32UI"},
      {GL_RGB32I, "GL_RGB32I"},
      {GL_RGBA8, "GL_RGBA8"},
      {GL_SRGB8_ALPHA8, "GL_SRGB8_ALPHA8"},
      {GL_RGBA8_SNORM, "GL_RGBA8_SNORM"},
      {GL_RGB10_A2, "GL_RGB10_A2"},
      {GL_RGBA16F, "GL_RGBA16F"},
      {GL_RGBA32F, "GL_RGBA32F"},
      {GL_RGBA8UI, "GL_RGBA8UI"},
      {GL_RGBA8I, "GL_RGBA8I"},
      {GL_RGB10_A2UI, "GL_RGB10_A2UI"},
      {GL_RGBA16UI, "GL_RGBA16UI"},
      {GL_RGBA16I, "GL_RGBA16I"},
      {GL_RGBA32UI, "GL_RGBA32UI"},
      {GL_RGBA32I, "GL_RGBA32I"},
      {GL_DEPTH_COMPONENT16, "GL_DEPTH_COMPONENT16"},
      {GL_DEPTH_COMPONENT24, "GL_DEPTH_COMPONENT24"},
      {GL_DEPTH_COMPONENT32F, "GL_DEPTH_COMPONENT32F"},
      {GL_DEPTH24_STENCIL8, "GL_DEPTH24_STENCIL8"},
      {GL_DEPTH32F_STENCIL8, "GL_DEPTH32F_STENCIL8"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureMagFilterMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NEAREST, "GL_NEAREST"},
      {GL_LINEAR, "GL_LINEAR"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureMinFilterMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NEAREST, "GL_NEAREST"},
      {GL_LINEAR, "GL_LINEAR"},
      {GL_NEAREST_MIPMAP_NEAREST, "GL_NEAREST_MIPMAP_NEAREST"},
      {GL_LINEAR_MIPMAP_NEAREST, "GL_LINEAR_MIPMAP_NEAREST"},
      {GL_NEAREST_MIPMAP_LINEAR, "GL_NEAREST_MIPMAP_LINEAR"},
      {GL_LINEAR_MIPMAP_LINEAR, "GL_LINEAR_MIPMAP_LINEAR"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_MAG_FILTER, "GL_TEXTURE_MAG_FILTER"},
      {GL_TEXTURE_MIN_FILTER, "GL_TEXTURE_MIN_FILTER"},
      {GL_TEXTURE_WRAP_S, "GL_TEXTURE_WRAP_S"},
      {GL_TEXTURE_WRAP_T, "GL_TEXTURE_WRAP_T"},
      {GL_TEXTURE_BASE_LEVEL, "GL_TEXTURE_BASE_LEVEL"},
      {GL_TEXTURE_COMPARE_FUNC, "GL_TEXTURE_COMPARE_FUNC"},
      {GL_TEXTURE_COMPARE_MODE, "GL_TEXTURE_COMPARE_MODE"},
      {GL_TEXTURE_IMMUTABLE_FORMAT, "GL_TEXTURE_IMMUTABLE_FORMAT"},
      {GL_TEXTURE_IMMUTABLE_LEVELS, "GL_TEXTURE_IMMUTABLE_LEVELS"},
      {GL_TEXTURE_MAX_LEVEL, "GL_TEXTURE_MAX_LEVEL"},
      {GL_TEXTURE_MAX_LOD, "GL_TEXTURE_MAX_LOD"},
      {GL_TEXTURE_MIN_LOD, "GL_TEXTURE_MIN_LOD"},
      {GL_TEXTURE_WRAP_R, "GL_TEXTURE_WRAP_R"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureSizedColorRenderableInternalFormat(
    uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_R8, "GL_R8"},
      {GL_R8UI, "GL_R8UI"},
      {GL_R8I, "GL_R8I"},
      {GL_R16UI, "GL_R16UI"},
      {GL_R16I, "GL_R16I"},
      {GL_R32UI, "GL_R32UI"},
      {GL_R32I, "GL_R32I"},
      {GL_RG8, "GL_RG8"},
      {GL_RG8UI, "GL_RG8UI"},
      {GL_RG8I, "GL_RG8I"},
      {GL_RG16UI, "GL_RG16UI"},
      {GL_RG16I, "GL_RG16I"},
      {GL_RG32UI, "GL_RG32UI"},
      {GL_RG32I, "GL_RG32I"},
      {GL_RGB8, "GL_RGB8"},
      {GL_RGB565, "GL_RGB565"},
      {GL_RGBA8, "GL_RGBA8"},
      {GL_SRGB8_ALPHA8, "GL_SRGB8_ALPHA8"},
      {GL_RGB5_A1, "GL_RGB5_A1"},
      {GL_RGBA4, "GL_RGBA4"},
      {GL_RGB10_A2, "GL_RGB10_A2"},
      {GL_RGBA8UI, "GL_RGBA8UI"},
      {GL_RGBA8I, "GL_RGBA8I"},
      {GL_RGB10_A2UI, "GL_RGB10_A2UI"},
      {GL_RGBA16UI, "GL_RGBA16UI"},
      {GL_RGBA16I, "GL_RGBA16I"},
      {GL_RGBA32UI, "GL_RGBA32UI"},
      {GL_RGBA32I, "GL_RGBA32I"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureSizedTextureFilterableInternalFormat(
    uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_R8, "GL_R8"},
      {GL_R8_SNORM, "GL_R8_SNORM"},
      {GL_R16F, "GL_R16F"},
      {GL_RG8, "GL_RG8"},
      {GL_RG8_SNORM, "GL_RG8_SNORM"},
      {GL_RG16F, "GL_RG16F"},
      {GL_RGB8, "GL_RGB8"},
      {GL_SRGB8, "GL_SRGB8"},
      {GL_RGB565, "GL_RGB565"},
      {GL_RGB8_SNORM, "GL_RGB8_SNORM"},
      {GL_R11F_G11F_B10F, "GL_R11F_G11F_B10F"},
      {GL_RGB9_E5, "GL_RGB9_E5"},
      {GL_RGB16F, "GL_RGB16F"},
      {GL_RGBA8, "GL_RGBA8"},
      {GL_SRGB8_ALPHA8, "GL_SRGB8_ALPHA8"},
      {GL_RGBA8_SNORM, "GL_RGBA8_SNORM"},
      {GL_RGB5_A1, "GL_RGB5_A1"},
      {GL_RGBA4, "GL_RGBA4"},
      {GL_RGB10_A2, "GL_RGB10_A2"},
      {GL_RGBA16F, "GL_RGBA16F"},
      {GL_R16_EXT, "GL_R16_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureSrgbDecodeExt(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_DECODE_EXT, "GL_DECODE_EXT"},
      {GL_SKIP_DECODE_EXT, "GL_SKIP_DECODE_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureStencilRenderableInternalFormat(
    uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_STENCIL_INDEX8, "GL_STENCIL_INDEX8"},
      {GL_DEPTH24_STENCIL8, "GL_DEPTH24_STENCIL8"},
      {GL_DEPTH32F_STENCIL8, "GL_DEPTH32F_STENCIL8"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureSwizzle(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_RED, "GL_RED"},     {GL_GREEN, "GL_GREEN"}, {GL_BLUE, "GL_BLUE"},
      {GL_ALPHA, "GL_ALPHA"}, {GL_ZERO, "GL_ZERO"},   {GL_ONE, "GL_ONE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TEXTURE_2D, "GL_TEXTURE_2D"},
      {GL_TEXTURE_CUBE_MAP_POSITIVE_X, "GL_TEXTURE_CUBE_MAP_POSITIVE_X"},
      {GL_TEXTURE_CUBE_MAP_NEGATIVE_X, "GL_TEXTURE_CUBE_MAP_NEGATIVE_X"},
      {GL_TEXTURE_CUBE_MAP_POSITIVE_Y, "GL_TEXTURE_CUBE_MAP_POSITIVE_Y"},
      {GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y"},
      {GL_TEXTURE_CUBE_MAP_POSITIVE_Z, "GL_TEXTURE_CUBE_MAP_POSITIVE_Z"},
      {GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureUnsizedInternalFormat(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_ALPHA, "GL_ALPHA"},
      {GL_LUMINANCE, "GL_LUMINANCE"},
      {GL_LUMINANCE_ALPHA, "GL_LUMINANCE_ALPHA"},
      {GL_RGB, "GL_RGB"},
      {GL_RGBA, "GL_RGBA"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureUsage(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_NONE, "GL_NONE"},
      {GL_FRAMEBUFFER_ATTACHMENT_ANGLE, "GL_FRAMEBUFFER_ATTACHMENT_ANGLE"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTextureWrapMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_CLAMP_TO_EDGE, "GL_CLAMP_TO_EDGE"},
      {GL_MIRRORED_REPEAT, "GL_MIRRORED_REPEAT"},
      {GL_REPEAT, "GL_REPEAT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTransformFeedbackBindTarget(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_TRANSFORM_FEEDBACK, "GL_TRANSFORM_FEEDBACK"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringTransformFeedbackPrimitiveMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_POINTS, "GL_POINTS"},
      {GL_LINES, "GL_LINES"},
      {GL_TRIANGLES, "GL_TRIANGLES"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringUniformBlockParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_UNIFORM_BLOCK_BINDING, "GL_UNIFORM_BLOCK_BINDING"},
      {GL_UNIFORM_BLOCK_DATA_SIZE, "GL_UNIFORM_BLOCK_DATA_SIZE"},
      {GL_UNIFORM_BLOCK_NAME_LENGTH, "GL_UNIFORM_BLOCK_NAME_LENGTH"},
      {GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, "GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS"},
      {GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
       "GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES"},
      {GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
       "GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER"},
      {GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
       "GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringUniformParameter(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_UNIFORM_SIZE, "GL_UNIFORM_SIZE"},
      {GL_UNIFORM_TYPE, "GL_UNIFORM_TYPE"},
      {GL_UNIFORM_NAME_LENGTH, "GL_UNIFORM_NAME_LENGTH"},
      {GL_UNIFORM_BLOCK_INDEX, "GL_UNIFORM_BLOCK_INDEX"},
      {GL_UNIFORM_OFFSET, "GL_UNIFORM_OFFSET"},
      {GL_UNIFORM_ARRAY_STRIDE, "GL_UNIFORM_ARRAY_STRIDE"},
      {GL_UNIFORM_MATRIX_STRIDE, "GL_UNIFORM_MATRIX_STRIDE"},
      {GL_UNIFORM_IS_ROW_MAJOR, "GL_UNIFORM_IS_ROW_MAJOR"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringVertexAttribIType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_BYTE, "GL_BYTE"},   {GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE"},
      {GL_SHORT, "GL_SHORT"}, {GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT"},
      {GL_INT, "GL_INT"},     {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringVertexAttribType(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_BYTE, "GL_BYTE"},
      {GL_UNSIGNED_BYTE, "GL_UNSIGNED_BYTE"},
      {GL_SHORT, "GL_SHORT"},
      {GL_UNSIGNED_SHORT, "GL_UNSIGNED_SHORT"},
      {GL_FLOAT, "GL_FLOAT"},
      {GL_INT, "GL_INT"},
      {GL_UNSIGNED_INT, "GL_UNSIGNED_INT"},
      {GL_HALF_FLOAT, "GL_HALF_FLOAT"},
      {GL_INT_2_10_10_10_REV, "GL_INT_2_10_10_10_REV"},
      {GL_UNSIGNED_INT_2_10_10_10_REV, "GL_UNSIGNED_INT_2_10_10_10_REV"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringVertexAttribute(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, "GL_VERTEX_ATTRIB_ARRAY_NORMALIZED"},
      {GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
       "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING"},
      {GL_VERTEX_ATTRIB_ARRAY_ENABLED, "GL_VERTEX_ATTRIB_ARRAY_ENABLED"},
      {GL_VERTEX_ATTRIB_ARRAY_SIZE, "GL_VERTEX_ATTRIB_ARRAY_SIZE"},
      {GL_VERTEX_ATTRIB_ARRAY_STRIDE, "GL_VERTEX_ATTRIB_ARRAY_STRIDE"},
      {GL_VERTEX_ATTRIB_ARRAY_TYPE, "GL_VERTEX_ATTRIB_ARRAY_TYPE"},
      {GL_CURRENT_VERTEX_ATTRIB, "GL_CURRENT_VERTEX_ATTRIB"},
      {GL_VERTEX_ATTRIB_ARRAY_INTEGER, "GL_VERTEX_ATTRIB_ARRAY_INTEGER"},
      {GL_VERTEX_ATTRIB_ARRAY_DIVISOR, "GL_VERTEX_ATTRIB_ARRAY_DIVISOR"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringVertexPointer(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_VERTEX_ATTRIB_ARRAY_POINTER, "GL_VERTEX_ATTRIB_ARRAY_POINTER"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

std::string GLES2Util::GetStringWindowRectanglesMode(uint32_t value) {
  static const EnumToString string_table[] = {
      {GL_INCLUSIVE_EXT, "GL_INCLUSIVE_EXT"},
      {GL_EXCLUSIVE_EXT, "GL_EXCLUSIVE_EXT"},
  };
  return GLES2Util::GetQualifiedEnumString(string_table,
                                           std::size(string_table), value);
}

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_IMPLEMENTATION_AUTOGEN_H_
