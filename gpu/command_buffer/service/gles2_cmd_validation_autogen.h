// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_AUTOGEN_H_

ValueValidator<GLenum> attachment;
ValueValidator<GLenum> attachment_query;
class BackbufferAttachmentValidator {
 public:
  bool IsValid(const GLenum value) const;
};
BackbufferAttachmentValidator backbuffer_attachment;

class BlitFilterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
BlitFilterValidator blit_filter;

class BufferModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
BufferModeValidator buffer_mode;

class BufferParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
  BufferParameterValidator();
  void SetIsES3(bool is_es3) { is_es3_ = is_es3; }

 private:
  bool is_es3_;
};
BufferParameterValidator buffer_parameter;

class BufferParameter64Validator {
 public:
  bool IsValid(const GLenum value) const;
};
BufferParameter64Validator buffer_parameter_64;

class BufferTargetValidator {
 public:
  bool IsValid(const GLenum value) const;
  BufferTargetValidator();
  void SetIsES3(bool is_es3) { is_es3_ = is_es3; }

 private:
  bool is_es3_;
};
BufferTargetValidator buffer_target;

class BufferUsageValidator {
 public:
  bool IsValid(const GLenum value) const;
  BufferUsageValidator();
  void SetIsES3(bool is_es3) { is_es3_ = is_es3; }

 private:
  bool is_es3_;
};
BufferUsageValidator buffer_usage;

ValueValidator<GLenum> bufferfi;
class BufferfvValidator {
 public:
  bool IsValid(const GLenum value) const;
};
BufferfvValidator bufferfv;

class BufferivValidator {
 public:
  bool IsValid(const GLenum value) const;
};
BufferivValidator bufferiv;

ValueValidator<GLenum> bufferuiv;
ValueValidator<GLenum> capability;
class CmpFunctionValidator {
 public:
  bool IsValid(const GLenum value) const;
};
CmpFunctionValidator cmp_function;

ValueValidator<GLenum> compressed_texture_format;
class DrawModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
DrawModeValidator draw_mode;

ValueValidator<GLenum> dst_blend_factor;
ValueValidator<GLenum> equation;
class FaceModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
FaceModeValidator face_mode;

class FaceTypeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
FaceTypeValidator face_type;

ValueValidator<GLenum> framebuffer_attachment_parameter;
ValueValidator<GLenum> framebuffer_parameter;
ValueValidator<GLenum> framebuffer_target;
ValueValidator<GLenum> g_l_state;
class GetMaxIndexTypeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
GetMaxIndexTypeValidator get_max_index_type;

ValueValidator<GLenum> get_tex_param_target;
class HintModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
HintModeValidator hint_mode;

ValueValidator<GLenum> hint_target;
ValueValidator<GLenum> image_internal_format;
ValueValidator<GLenum> index_type;
class IndexedBufferTargetValidator {
 public:
  bool IsValid(const GLenum value) const;
};
IndexedBufferTargetValidator indexed_buffer_target;

ValueValidator<GLenum> indexed_g_l_state;
class InternalFormatParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
InternalFormatParameterValidator internal_format_parameter;

class MapBufferAccessValidator {
 public:
  bool IsValid(const GLenum value) const;
};
MapBufferAccessValidator map_buffer_access;

ValueValidator<GLenum> pixel_store;
class PixelStoreAlignmentValidator {
 public:
  bool IsValid(const GLint value) const;
};
PixelStoreAlignmentValidator pixel_store_alignment;

ValueValidator<GLenum> pixel_type;
ValueValidator<GLenum> program_parameter;
class QueryObjectParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
QueryObjectParameterValidator query_object_parameter;

class QueryTargetValidator {
 public:
  bool IsValid(const GLenum value) const;
};
QueryTargetValidator query_target;

ValueValidator<GLenum> read_buffer;
ValueValidator<GLenum> read_pixel_format;
ValueValidator<GLenum> read_pixel_type;
ValueValidator<GLenum> render_buffer_format;
ValueValidator<GLenum> render_buffer_parameter;
ValueValidator<GLenum> render_buffer_target;
class ResetStatusValidator {
 public:
  bool IsValid(const GLenum value) const;
};
ResetStatusValidator reset_status;

ValueValidator<GLenum> sampler_parameter;
ValueValidator<GLenum> shader_binary_format;
ValueValidator<GLenum> shader_parameter;
class ShaderPrecisionValidator {
 public:
  bool IsValid(const GLenum value) const;
};
ShaderPrecisionValidator shader_precision;

class ShaderTypeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
ShaderTypeValidator shader_type;

class SharedImageAccessModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
SharedImageAccessModeValidator shared_image_access_mode;

ValueValidator<GLenum> src_blend_factor;
class StencilOpValidator {
 public:
  bool IsValid(const GLenum value) const;
};
StencilOpValidator stencil_op;

class StringTypeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
StringTypeValidator string_type;

class SwapBuffersFlagsValidator {
 public:
  bool IsValid(const GLbitfield value) const;
};
SwapBuffersFlagsValidator swap_buffers_flags;

ValueValidator<GLbitfield> sync_flush_flags;
class SyncParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
SyncParameterValidator sync_parameter;

class Texture3DTargetValidator {
 public:
  bool IsValid(const GLenum value) const;
};
Texture3DTargetValidator texture_3_d_target;

ValueValidator<GLenum> texture_bind_target;
class TextureCompareFuncValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureCompareFuncValidator texture_compare_func;

ValueValidator<GLenum> texture_compare_mode;
ValueValidator<GLenum> texture_depth_renderable_internal_format;
ValueValidator<GLenum> texture_fbo_target;
ValueValidator<GLenum> texture_format;
ValueValidator<GLenum> texture_internal_format;
ValueValidator<GLenum> texture_internal_format_storage;
class TextureMagFilterModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureMagFilterModeValidator texture_mag_filter_mode;

class TextureMinFilterModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureMinFilterModeValidator texture_min_filter_mode;

ValueValidator<GLenum> texture_parameter;
ValueValidator<GLenum> texture_sized_color_renderable_internal_format;
ValueValidator<GLenum> texture_sized_texture_filterable_internal_format;
class TextureSrgbDecodeExtValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureSrgbDecodeExtValidator texture_srgb_decode_ext;

ValueValidator<GLenum> texture_stencil_renderable_internal_format;
class TextureSwizzleValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureSwizzleValidator texture_swizzle;

ValueValidator<GLenum> texture_target;
ValueValidator<GLenum> texture_unsized_internal_format;
class TextureUsageValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureUsageValidator texture_usage;

class TextureWrapModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TextureWrapModeValidator texture_wrap_mode;

ValueValidator<GLenum> transform_feedback_bind_target;
class TransformFeedbackPrimitiveModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
TransformFeedbackPrimitiveModeValidator transform_feedback_primitive_mode;

class UniformBlockParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
UniformBlockParameterValidator uniform_block_parameter;

class UniformParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
UniformParameterValidator uniform_parameter;

class VertexAttribITypeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
VertexAttribITypeValidator vertex_attrib_i_type;

ValueValidator<GLenum> vertex_attrib_type;
ValueValidator<GLenum> vertex_attribute;
ValueValidator<GLenum> vertex_pointer;
class WindowRectanglesModeValidator {
 public:
  bool IsValid(const GLenum value) const;
};
WindowRectanglesModeValidator window_rectangles_mode;

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_VALIDATION_AUTOGEN_H_
