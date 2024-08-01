// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the ContextState class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/sampler_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class Buffer;
class FeatureInfo;
class IndexedBufferBindingHost;
class Program;
class Renderbuffer;
class TransformFeedback;

// State associated with each texture unit.
struct GPU_GLES2_EXPORT TextureUnit {
  TextureUnit();
  TextureUnit(const TextureUnit& other);
  ~TextureUnit();

  // The last target that was bound to this texture unit.
  GLenum bind_target;

  // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
  scoped_refptr<TextureRef> bound_texture_2d;

  // texture currently bound to this unit's GL_TEXTURE_CUBE_MAP with
  // glBindTexture
  scoped_refptr<TextureRef> bound_texture_cube_map;

  // texture currently bound to this unit's GL_TEXTURE_EXTERNAL_OES with
  // glBindTexture
  scoped_refptr<TextureRef> bound_texture_external_oes;

  // texture currently bound to this unit's GL_TEXTURE_RECTANGLE_ARB with
  // glBindTexture
  scoped_refptr<TextureRef> bound_texture_rectangle_arb;

  // texture currently bound to this unit's GL_TEXTURE_3D with glBindTexture
  scoped_refptr<TextureRef> bound_texture_3d;

  // texture currently bound to this unit's GL_TEXTURE_2D_ARRAY with
  // glBindTexture
  scoped_refptr<TextureRef> bound_texture_2d_array;

  bool AnyTargetBound() const {
    return bound_texture_2d || bound_texture_cube_map ||
           bound_texture_external_oes || bound_texture_rectangle_arb ||
           bound_texture_3d || bound_texture_2d_array;
  }

  TextureRef* GetInfoForSamplerType(GLenum type) {
    switch (type) {
      case GL_SAMPLER_2D:
      case GL_SAMPLER_2D_SHADOW:
      case GL_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_2D:
        return bound_texture_2d.get();
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
        return bound_texture_cube_map.get();
      case GL_SAMPLER_EXTERNAL_OES:
        return bound_texture_external_oes.get();
      case GL_SAMPLER_2D_RECT_ARB:
        return bound_texture_rectangle_arb.get();
      case GL_SAMPLER_3D:
      case GL_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_3D:
        return bound_texture_3d.get();
      case GL_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        return bound_texture_2d_array.get();
      default:
        NOTREACHED_IN_MIGRATION();
        return nullptr;
    }
  }

  TextureRef* GetInfoForTarget(GLenum target) {
    switch (target) {
      case GL_TEXTURE_2D:
        return bound_texture_2d.get();
      case GL_TEXTURE_CUBE_MAP:
        return bound_texture_cube_map.get();
      case GL_TEXTURE_EXTERNAL_OES:
        return bound_texture_external_oes.get();
      case GL_TEXTURE_RECTANGLE_ARB:
        return bound_texture_rectangle_arb.get();
      case GL_TEXTURE_3D:
        return bound_texture_3d.get();
      case GL_TEXTURE_2D_ARRAY:
        return bound_texture_2d_array.get();
      default:
        NOTREACHED_IN_MIGRATION();
        return nullptr;
    }
  }

  void SetInfoForTarget(GLenum target, TextureRef* texture_ref) {
    switch (target) {
      case GL_TEXTURE_2D:
        bound_texture_2d = texture_ref;
        break;
      case GL_TEXTURE_CUBE_MAP:
        bound_texture_cube_map = texture_ref;
        break;
      case GL_TEXTURE_EXTERNAL_OES:
        bound_texture_external_oes = texture_ref;
        break;
      case GL_TEXTURE_RECTANGLE_ARB:
        bound_texture_rectangle_arb = texture_ref;
        break;
      case GL_TEXTURE_3D:
        bound_texture_3d = texture_ref;
        break;
      case GL_TEXTURE_2D_ARRAY:
        bound_texture_2d_array = texture_ref;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
};

class GPU_GLES2_EXPORT Vec4 {
 public:
  Vec4() {
    v_[0].float_value = 0.0f;
    v_[1].float_value = 0.0f;
    v_[2].float_value = 0.0f;
    v_[3].float_value = 1.0f;
    type_ = SHADER_VARIABLE_FLOAT;
  }

  template <typename T>
  void GetValues(T* values) const;

  template <typename T>
  void SetValues(const T* values);

  ShaderVariableBaseType type() const { return type_; }

  bool Equal(const Vec4& other) const;

 private:
  union ValueUnion {
    GLfloat float_value;
    GLint int_value;
    GLuint uint_value;
  };

  ValueUnion v_[4];
  ShaderVariableBaseType type_;
};

template <>
GPU_GLES2_EXPORT void Vec4::GetValues<GLfloat>(GLfloat* values) const;
template <>
GPU_GLES2_EXPORT void Vec4::GetValues<GLint>(GLint* values) const;
template <>
GPU_GLES2_EXPORT void Vec4::GetValues<GLuint>(GLuint* values) const;

template <>
GPU_GLES2_EXPORT void Vec4::SetValues<GLfloat>(const GLfloat* values);
template <>
GPU_GLES2_EXPORT void Vec4::SetValues<GLint>(const GLint* values);
template <>
GPU_GLES2_EXPORT void Vec4::SetValues<GLuint>(const GLuint* values);

struct GPU_GLES2_EXPORT ContextState {
  enum Dimension { k2D, k3D };

  ContextState(FeatureInfo* feature_info,
               bool track_texture_and_sampler_units = true);
  ~ContextState();

  void set_api(gl::GLApi* api) { api_ = api; }
  gl::GLApi* api() const { return api_; }

  void Initialize();

  void MarkContextLost() { context_lost_ = true; }

  void SetLineWidthBounds(GLfloat min, GLfloat max);

  void SetIgnoreCachedStateForTest(bool ignore) {
    ignore_cached_state = ignore;
  }

  void RestoreState(const ContextState* prev_state);
  void InitCapabilities(const ContextState* prev_state) const;
  void InitState(const ContextState* prev_state) const;

  void RestoreActiveTexture() const;
  void RestoreAllTextureUnitAndSamplerBindings(
      const ContextState* prev_state) const;
  void RestoreActiveTextureUnitBinding(unsigned int target) const;
  void RestoreVertexAttribValues() const;
  void RestoreVertexAttribArrays(
      const scoped_refptr<VertexAttribManager> attrib_manager) const;
  void RestoreVertexAttribs(const ContextState* prev_state) const;
  void RestoreBufferBindings() const;
  void RestoreGlobalState(const ContextState* prev_state) const;
  void RestoreProgramSettings(const ContextState* prev_state,
                              bool restore_transform_feedback_bindings) const;
  void RestoreRenderbufferBindings();
  void RestoreIndexedUniformBufferBindings(const ContextState* prev_state);
  void RestoreTextureUnitBindings(GLuint unit,
                                  const ContextState* prev_state) const;
  void RestoreSamplerBinding(GLuint unit, const ContextState* prev_state) const;

  void PushTextureUnpackState() const;
  void RestoreUnpackState() const;
  void DoLineWidth(GLfloat width) const;

  // Helper for getting cached state.
  bool GetStateAsGLint(GLenum pname, GLint* params, GLsizei* num_written) const;
  bool GetStateAsGLfloat(GLenum pname,
                         GLfloat* params,
                         GLsizei* num_written) const;
  bool GetEnabled(GLenum cap) const;

  inline void SetDeviceColorMask(GLboolean red,
                                 GLboolean green,
                                 GLboolean blue,
                                 GLboolean alpha) {
    if (cached_color_mask_red == red && cached_color_mask_green == green &&
        cached_color_mask_blue == blue && cached_color_mask_alpha == alpha &&
        !ignore_cached_state)
      return;
    cached_color_mask_red = red;
    cached_color_mask_green = green;
    cached_color_mask_blue = blue;
    cached_color_mask_alpha = alpha;
    api()->glColorMaskFn(red, green, blue, alpha);
  }

  inline void SetDeviceDepthMask(GLboolean mask) {
    if (cached_depth_mask == mask && !ignore_cached_state)
      return;
    cached_depth_mask = mask;
    api()->glDepthMaskFn(mask);
  }

  inline void SetDeviceStencilMaskSeparate(GLenum op, GLuint mask) {
    if (op == GL_FRONT) {
      if (cached_stencil_front_writemask == mask && !ignore_cached_state)
        return;
      cached_stencil_front_writemask = mask;
    } else if (op == GL_BACK) {
      if (cached_stencil_back_writemask == mask && !ignore_cached_state)
        return;
      cached_stencil_back_writemask = mask;
    } else {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    api()->glStencilMaskSeparateFn(op, mask);
  }

  void SetBoundBuffer(GLenum target, Buffer* buffer);
  void RemoveBoundBuffer(Buffer* buffer);

  void InitGenericAttribs(GLuint max_vertex_attribs) {
    attrib_values.resize(max_vertex_attribs);

    uint32_t packed_size = max_vertex_attribs / 16;
    packed_size += (max_vertex_attribs % 16 == 0) ? 0 : 1;
    generic_attrib_base_type_mask_.resize(packed_size);
    for (uint32_t i = 0; i < packed_size; ++i) {
      // All generic attribs are float type by default.
      generic_attrib_base_type_mask_[i] = 0x55555555u * SHADER_VARIABLE_FLOAT;
    }
  }

  void SetGenericVertexAttribBaseType(GLuint index, GLenum base_type) {
    DCHECK_LT(index, attrib_values.size());
    int shift_bits = (index % 16) * 2;
    generic_attrib_base_type_mask_[index / 16] &= ~(0x3 << shift_bits);
    generic_attrib_base_type_mask_[index / 16] |= (base_type << shift_bits);
  }

  const std::vector<uint32_t>& generic_attrib_base_type_mask() const {
    return generic_attrib_base_type_mask_;
  }

  void UnbindTexture(TextureRef* texture);
  void UnbindSampler(Sampler* sampler);

  PixelStoreParams GetPackParams();
  PixelStoreParams GetUnpackParams(Dimension dimension);

  // If a buffer object is bound to PIXEL_PACK_BUFFER, set all pack parameters
  // user values; otherwise, set them to 0.
  void UpdatePackParameters() const;
  // If a buffer object is bound to PIXEL_UNPACK_BUFFER, set all unpack
  // parameters user values; otherwise, set them to 0.
  void UpdateUnpackParameters() const;

  void SetMaxWindowRectangles(size_t max);
  size_t GetMaxWindowRectangles() const;
  void SetWindowRectangles(GLenum mode,
                           size_t count,
                           const volatile GLint* box);
  template <typename T>
  void GetWindowRectangle(GLuint index, T* box) {
    for (size_t i = 0; i < 4; ++i) {
      box[i] = window_rectangles_[4 * index + i];
    }
  }
  void UpdateWindowRectangles() const;
  void UpdateWindowRectanglesForBoundDrawFramebufferClientID(GLuint client_id);

  void EnableDisableFramebufferSRGB(bool enable);

#include "gpu/command_buffer/service/context_state_autogen.h"

  // if false, we will not track individual texture and sampler units, instead
  // we only track if all units are in ground state or not.
  const bool track_texture_and_sampler_units;

  EnableFlags enable_flags;

  // Current active texture by 0 - n index.
  // In other words, if we call glActiveTexture(GL_TEXTURE2) this value would
  // be 2.
  GLuint active_texture_unit = 0u;

  // The currently bound array buffer. If this is 0 it is illegal to call
  // glVertexAttribPointer.
  scoped_refptr<Buffer> bound_array_buffer;

  scoped_refptr<Buffer> bound_copy_read_buffer;
  scoped_refptr<Buffer> bound_copy_write_buffer;
  scoped_refptr<Buffer> bound_pixel_pack_buffer;
  scoped_refptr<Buffer> bound_pixel_unpack_buffer;
  scoped_refptr<Buffer> bound_transform_feedback_buffer;
  scoped_refptr<Buffer> bound_uniform_buffer;

  // Which textures are bound to texture units through glActiveTexture.
  std::vector<TextureUnit> texture_units;
  mutable bool texture_units_in_ground_state = true;

  // Which samplers are bound to each texture unit;
  std::vector<scoped_refptr<Sampler>> sampler_units;

  // We create a transform feedback as the default one per ES3 enabled context
  // instead of using GL's default one to make context switching easier.
  // For other context, we will never change the default transform feedback's
  // states, so we can just use the GL's default one.
  scoped_refptr<TransformFeedback> default_transform_feedback;

  scoped_refptr<TransformFeedback> bound_transform_feedback;

  scoped_refptr<IndexedBufferBindingHost> indexed_uniform_buffer_bindings;

  // The values for each attrib.
  std::vector<Vec4> attrib_values;

  // Class that manages vertex attribs.
  scoped_refptr<VertexAttribManager> vertex_attrib_manager;
  scoped_refptr<VertexAttribManager> default_vertex_attrib_manager;

  // The program in use by glUseProgram
  scoped_refptr<Program> current_program;

  // The currently bound renderbuffer
  scoped_refptr<Renderbuffer> bound_renderbuffer;
  bool bound_renderbuffer_valid = false;

  bool pack_reverse_row_order = false;
  bool ignore_cached_state = false;

  mutable bool fbo_binding_for_scissor_workaround_dirty = false;
  mutable bool stencil_state_changed_since_validation = true;

  GLuint current_draw_framebuffer_client_id = 0;

 private:
  void EnableDisable(GLenum pname, bool enable) const;

  void InitStateManual(const ContextState* prev_state) const;

  // EnableDisableFramebufferSRGB is called at very high frequency. Cache the
  // true value of FRAMEBUFFER_SRGB, if we know it, to elide some of these
  // calls.
  bool framebuffer_srgb_valid_ = false;
  bool framebuffer_srgb_ = false;

  // Generic vertex attrib base types: FLOAT, INT, or UINT.
  // Each base type is encoded into 2 bits, the lowest 2 bits for location 0,
  // the highest 2 bits for location (max_vertex_attribs - 1).
  std::vector<uint32_t> generic_attrib_base_type_mask_;

  GLfloat line_width_min_ = 0.0f;
  GLfloat line_width_max_ = 1.0f;

  // Stores the list of N window rectangles as N*4 GLints, like
  // vector<[x,y,w,h]>. Always has space for MAX_WINDOW_RECTANGLES rectangles.
  std::vector<GLint> window_rectangles_;

  raw_ptr<gl::GLApi, DanglingUntriaged> api_ = nullptr;
  raw_ptr<FeatureInfo, DanglingUntriaged> feature_info_;

  bool context_lost_ = false;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_
