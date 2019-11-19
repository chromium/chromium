// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include <stddef.h>

#include <cmath>

#include "base/numerics/ranges.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {

GLuint Get2dServiceId(const TextureUnit& unit) {
  return unit.bound_texture_2d.get() ? unit.bound_texture_2d->service_id() : 0;
}

GLuint Get2dArrayServiceId(const TextureUnit& unit) {
  return unit.bound_texture_2d_array.get()
             ? unit.bound_texture_2d_array->service_id()
             : 0;
}

GLuint Get3dServiceId(const TextureUnit& unit) {
  return unit.bound_texture_3d.get() ? unit.bound_texture_3d->service_id() : 0;
}

GLuint GetCubeServiceId(const TextureUnit& unit) {
  return unit.bound_texture_cube_map.get()
             ? unit.bound_texture_cube_map->service_id()
             : 0;
}

GLuint GetOesServiceId(const TextureUnit& unit) {
  return unit.bound_texture_external_oes.get()
             ? unit.bound_texture_external_oes->service_id()
             : 0;
}

GLuint GetArbServiceId(const TextureUnit& unit) {
  return unit.bound_texture_rectangle_arb.get()
             ? unit.bound_texture_rectangle_arb->service_id()
             : 0;
}

GLuint GetServiceId(const TextureUnit& unit, GLuint target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return Get2dServiceId(unit);
    case GL_TEXTURE_CUBE_MAP:
      return GetCubeServiceId(unit);
    case GL_TEXTURE_RECTANGLE_ARB:
      return GetArbServiceId(unit);
    case GL_TEXTURE_EXTERNAL_OES:
      return GetOesServiceId(unit);
    default:
      NOTREACHED();
      return 0;
  }
}

bool TargetIsSupported(const FeatureInfo* feature_info, GLuint target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return true;
    case GL_TEXTURE_CUBE_MAP:
      return true;
    case GL_TEXTURE_RECTANGLE_ARB:
      return feature_info->feature_flags().arb_texture_rectangle;
    case GL_TEXTURE_EXTERNAL_OES:
      return feature_info->feature_flags().oes_egl_image_external ||
             feature_info->feature_flags().nv_egl_stream_consumer_external;
    default:
      NOTREACHED();
      return false;
  }
}

GLuint GetBufferId(const Buffer* buffer) {
  if (buffer)
    return buffer->service_id();
  return 0;
}

}  // anonymous namespace.

TextureUnit::TextureUnit() : bind_target(GL_TEXTURE_2D) {}

TextureUnit::TextureUnit(const TextureUnit& other) = default;

TextureUnit::~TextureUnit() = default;

bool Vec4::Equal(const Vec4& other) const {
  if (type_ != other.type_)
    return false;
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii) {
        if (v_[ii].float_value != other.v_[ii].float_value)
          return false;
      }
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii) {
        if (v_[ii].int_value != other.v_[ii].int_value)
          return false;
      }
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii) {
        if (v_[ii].uint_value != other.v_[ii].uint_value)
          return false;
      }
      break;
    default:
      NOTREACHED();
      break;
  }
  return true;
}

template <>
void Vec4::GetValues<GLfloat>(GLfloat* values) const {
  DCHECK(values);
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = v_[ii].float_value;
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLfloat>(v_[ii].int_value);
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLfloat>(v_[ii].uint_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
void Vec4::GetValues<GLint>(GLint* values) const {
  DCHECK(values);
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLint>(v_[ii].float_value);
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = v_[ii].int_value;
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLint>(v_[ii].uint_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
void Vec4::GetValues<GLuint>(GLuint* values) const {
  DCHECK(values);
  switch (type_) {
    case SHADER_VARIABLE_FLOAT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLuint>(v_[ii].float_value);
      break;
    case SHADER_VARIABLE_INT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = static_cast<GLuint>(v_[ii].int_value);
      break;
    case SHADER_VARIABLE_UINT:
      for (size_t ii = 0; ii < 4; ++ii)
        values[ii] = v_[ii].uint_value;
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
void Vec4::SetValues<GLfloat>(const GLfloat* values) {
  DCHECK(values);
  for (size_t ii = 0; ii < 4; ++ii)
    v_[ii].float_value = values[ii];
  type_ = SHADER_VARIABLE_FLOAT;
}

template <>
void Vec4::SetValues<GLint>(const GLint* values) {
  DCHECK(values);
  for (size_t ii = 0; ii < 4; ++ii)
    v_[ii].int_value = values[ii];
  type_ = SHADER_VARIABLE_INT;
}

template <>
void Vec4::SetValues<GLuint>(const GLuint* values) {
  DCHECK(values);
  for (size_t ii = 0; ii < 4; ++ii)
    v_[ii].uint_value = values[ii];
  type_ = SHADER_VARIABLE_UINT;
}

ContextState::ContextState(FeatureInfo* feature_info,
                           bool track_texture_and_sampler_units)
    : track_texture_and_sampler_units(track_texture_and_sampler_units),
      feature_info_(feature_info) {
  Initialize();
}

ContextState::~ContextState() = default;

void ContextState::SetLineWidthBounds(GLfloat min, GLfloat max) {
  line_width_min_ = min;
  line_width_max_ = max;
}

void ContextState::RestoreTextureUnitBindings(
    GLuint unit,
    const ContextState* prev_state) const {
  DCHECK(unit < texture_units.size() ||
         (unit == 0 && !track_texture_and_sampler_units));

  GLuint service_id_2d = 0u;
  GLuint service_id_2d_array = 0u;
  GLuint service_id_3d = 0u;
  GLuint service_id_cube = 0u;
  GLuint service_id_oes = 0u;
  GLuint service_id_arb = 0u;

  if (track_texture_and_sampler_units) {
    const TextureUnit& texture_unit = texture_units[unit];
    service_id_2d = Get2dServiceId(texture_unit);
    service_id_2d_array = Get2dArrayServiceId(texture_unit);
    service_id_3d = Get3dServiceId(texture_unit);
    service_id_cube = GetCubeServiceId(texture_unit);
    service_id_oes = GetOesServiceId(texture_unit);
    service_id_arb = GetArbServiceId(texture_unit);
  }

  bool bind_texture_2d = true;
  bool bind_texture_cube = true;
  bool bind_texture_oes =
      feature_info_->feature_flags().oes_egl_image_external ||
      feature_info_->feature_flags().nv_egl_stream_consumer_external;
  bool bind_texture_arb = feature_info_->feature_flags().arb_texture_rectangle;
  // TEXTURE_2D_ARRAY and TEXTURE_3D are only applicable from ES3 version.
  // So set it to FALSE by default.
  bool bind_texture_2d_array = false;
  bool bind_texture_3d = false;
  // set the variables to true only if the application is ES3 or newer
  if (feature_info_->IsES3Capable()) {
    bind_texture_2d_array = true;
    bind_texture_3d = true;
  }

  if (prev_state) {
    if (prev_state->track_texture_and_sampler_units) {
      const TextureUnit& prev_unit = prev_state->texture_units[unit];
      bind_texture_2d = service_id_2d != Get2dServiceId(prev_unit);
      bind_texture_2d_array =
          bind_texture_2d_array &&
          service_id_2d_array != Get2dArrayServiceId(prev_unit);
      bind_texture_3d =
          bind_texture_3d && service_id_3d != Get3dServiceId(prev_unit);
      bind_texture_cube = service_id_cube != GetCubeServiceId(prev_unit);
      bind_texture_oes =
          bind_texture_oes && service_id_oes != GetOesServiceId(prev_unit);
      bind_texture_arb =
          bind_texture_arb && service_id_arb != GetArbServiceId(prev_unit);
    } else if (prev_state->texture_units_in_ground_state) {
      bind_texture_2d = service_id_2d;
      bind_texture_2d_array = bind_texture_2d_array && service_id_2d_array;
      bind_texture_3d = bind_texture_3d && service_id_3d;
      bind_texture_cube = service_id_cube;
      bind_texture_oes = bind_texture_oes && service_id_oes;
      bind_texture_arb = bind_texture_arb && service_id_arb;
    } else {
      // We need bind all restore target binding, if texture units is not in
      // ground state.
    }
  }

  // Early-out if nothing has changed from the previous state.
  if (!bind_texture_2d && !bind_texture_2d_array && !bind_texture_3d &&
      !bind_texture_cube && !bind_texture_oes && !bind_texture_arb) {
    return;
  }

  api()->glActiveTextureFn(GL_TEXTURE0 + unit);
  if (bind_texture_2d) {
    api()->glBindTextureFn(GL_TEXTURE_2D, service_id_2d);
  }
  if (bind_texture_cube) {
    api()->glBindTextureFn(GL_TEXTURE_CUBE_MAP, service_id_cube);
  }
  if (bind_texture_oes) {
    api()->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES, service_id_oes);
  }
  if (bind_texture_arb) {
    api()->glBindTextureFn(GL_TEXTURE_RECTANGLE_ARB, service_id_arb);
  }
  if (bind_texture_2d_array) {
    api()->glBindTextureFn(GL_TEXTURE_2D_ARRAY, service_id_2d_array);
  }
  if (bind_texture_3d) {
    api()->glBindTextureFn(GL_TEXTURE_3D, service_id_3d);
  }
}  // namespace gles2

void ContextState::RestoreSamplerBinding(GLuint unit,
                                         const ContextState* prev_state) const {
  if (!feature_info_->IsES3Capable())
    return;
  const scoped_refptr<Sampler>& cur_sampler = sampler_units[unit];
  GLuint cur_id = cur_sampler ? cur_sampler->service_id() : 0;
  GLuint prev_id = 0;
  if (prev_state && prev_state->track_texture_and_sampler_units) {
    const scoped_refptr<Sampler>& prev_sampler =
        prev_state->sampler_units[unit];
    prev_id = prev_sampler ? prev_sampler->service_id() : 0;
  }
  if (!prev_state || !prev_state->sampler_units_in_ground_state ||
      cur_id != prev_id) {
    api()->glBindSamplerFn(unit, cur_id);
  }
}

void ContextState::PushTextureUnpackState() const {
  api()->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, 1);

  if (bound_pixel_unpack_buffer.get()) {
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
    api()->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, 0);
    api()->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, 0);
    DCHECK_EQ(0, unpack_skip_pixels);
    DCHECK_EQ(0, unpack_skip_rows);
    DCHECK_EQ(0, unpack_skip_images);
  }
}

void ContextState::RestoreUnpackState() const {
  api()->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, unpack_alignment);
  if (bound_pixel_unpack_buffer.get()) {
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER,
                          GetBufferId(bound_pixel_unpack_buffer.get()));
    api()->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, unpack_row_length);
    api()->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, unpack_image_height);
  }
}

void ContextState::DoLineWidth(GLfloat width) const {
  api()->glLineWidthFn(
      base::ClampToRange(width, line_width_min_, line_width_max_));
}

void ContextState::RestoreBufferBindings() const {
  if (vertex_attrib_manager.get()) {
    Buffer* element_array_buffer =
        vertex_attrib_manager->element_array_buffer();
    api()->glBindBufferFn(GL_ELEMENT_ARRAY_BUFFER,
                          GetBufferId(element_array_buffer));
  }
  api()->glBindBufferFn(GL_ARRAY_BUFFER, GetBufferId(bound_array_buffer.get()));
  if (feature_info_->IsES3Capable()) {
    api()->glBindBufferFn(GL_COPY_READ_BUFFER,
                          GetBufferId(bound_copy_read_buffer.get()));
    api()->glBindBufferFn(GL_COPY_WRITE_BUFFER,
                          GetBufferId(bound_copy_write_buffer.get()));
    api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER,
                          GetBufferId(bound_pixel_pack_buffer.get()));
    UpdatePackParameters();
    api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER,
                          GetBufferId(bound_pixel_unpack_buffer.get()));
    UpdateUnpackParameters();
    api()->glBindBufferFn(GL_TRANSFORM_FEEDBACK_BUFFER,
                          GetBufferId(bound_transform_feedback_buffer.get()));
    api()->glBindBufferFn(GL_UNIFORM_BUFFER,
                          GetBufferId(bound_uniform_buffer.get()));
  }
}

void ContextState::RestoreRenderbufferBindings() {
  // Require Renderbuffer rebind.
  bound_renderbuffer_valid = false;
}

void ContextState::RestoreProgramSettings(
    const ContextState* prev_state,
    bool restore_transform_feedback_bindings) const {
  bool flag =
      (restore_transform_feedback_bindings && feature_info_->IsES3Capable());
  if (flag && prev_state) {
    if (prev_state->bound_transform_feedback.get() &&
        prev_state->bound_transform_feedback->active() &&
        !prev_state->bound_transform_feedback->paused()) {
      api()->glPauseTransformFeedbackFn();
    }
  }
  api()->glUseProgramFn(current_program.get() ? current_program->service_id()
                                              : 0);
  if (flag) {
    if (bound_transform_feedback.get()) {
      bound_transform_feedback->DoBindTransformFeedback(
          GL_TRANSFORM_FEEDBACK, bound_transform_feedback.get(),
          bound_transform_feedback_buffer.get());
    } else {
      api()->glBindTransformFeedbackFn(GL_TRANSFORM_FEEDBACK, 0);
    }
  }
}

void ContextState::RestoreIndexedUniformBufferBindings(
    const ContextState* prev_state) {
  if (!feature_info_->IsES3Capable())
    return;
  indexed_uniform_buffer_bindings->RestoreBindings(
      prev_state ? prev_state->indexed_uniform_buffer_bindings.get() : nullptr);
}

void ContextState::RestoreActiveTexture() const {
  api()->glActiveTextureFn(GL_TEXTURE0 + active_texture_unit);
}

void ContextState::RestoreAllTextureUnitAndSamplerBindings(
    const ContextState* prev_state) const {
  if (!track_texture_and_sampler_units) {
    if (prev_state) {
      if (!prev_state->track_texture_and_sampler_units) {
        texture_units_in_ground_state =
            prev_state->texture_units_in_ground_state;
        sampler_units_in_ground_state =
            prev_state->sampler_units_in_ground_state;
        return;
      }

      texture_units_in_ground_state = true;
      for (size_t i = 1; i < prev_state->texture_units.size(); ++i) {
        if (prev_state->texture_units[i].AnyTargetBound()) {
          texture_units_in_ground_state = false;
          break;
        }
      }

      // If the current gl texture units are not in ground state, then we do
      // need reset texture unit 0.
      if (texture_units_in_ground_state)
        RestoreTextureUnitBindings(0, prev_state);

      sampler_units_in_ground_state = true;
      for (auto& sampler : prev_state->sampler_units) {
        if (sampler) {
          sampler_units_in_ground_state = false;
          break;
        }
      }
    } else {
      texture_units_in_ground_state = false;
      sampler_units_in_ground_state = false;
    }
  } else {
    // Restore Texture state.
    for (size_t i = 0; i < texture_units.size(); ++i) {
      RestoreTextureUnitBindings(i, prev_state);
      RestoreSamplerBinding(i, prev_state);
    }
    RestoreActiveTexture();
  }
}

void ContextState::RestoreActiveTextureUnitBinding(unsigned int target) const {
  DCHECK(active_texture_unit < texture_units.size() ||
         (active_texture_unit == 0 && !track_texture_and_sampler_units));
  GLuint service_id = 0;
  if (track_texture_and_sampler_units) {
    const TextureUnit& texture_unit = texture_units[active_texture_unit];
    service_id = GetServiceId(texture_unit, target);
  }
  if (TargetIsSupported(feature_info_, target))
    api()->glBindTextureFn(target, service_id);
}

void ContextState::RestoreVertexAttribValues() const {
  for (size_t attrib = 0; attrib < vertex_attrib_manager->num_attribs();
       ++attrib) {
    switch (attrib_values[attrib].type()) {
      case SHADER_VARIABLE_FLOAT: {
        GLfloat v[4];
        attrib_values[attrib].GetValues(v);
        api()->glVertexAttrib4fvFn(attrib, v);
      } break;
      case SHADER_VARIABLE_INT: {
        GLint v[4];
        attrib_values[attrib].GetValues(v);
        api()->glVertexAttribI4ivFn(attrib, v);
      } break;
      case SHADER_VARIABLE_UINT: {
        GLuint v[4];
        attrib_values[attrib].GetValues(v);
        api()->glVertexAttribI4uivFn(attrib, v);
      } break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void ContextState::RestoreVertexAttribArrays(
    const scoped_refptr<VertexAttribManager> attrib_manager) const {
  // This is expected to be called only for VAO with service_id 0,
  // either to restore the default VAO or a virtual VAO with service_id 0.
  GLuint vao_service_id = attrib_manager->service_id();
  DCHECK(vao_service_id == 0);

  // Bind VAO if supported.
  if (feature_info_->feature_flags().native_vertex_array_object)
    api()->glBindVertexArrayOESFn(vao_service_id);

  // Restore vertex attrib arrays.
  for (size_t attrib_index = 0; attrib_index < attrib_manager->num_attribs();
       ++attrib_index) {
    const VertexAttrib* attrib = attrib_manager->GetVertexAttrib(attrib_index);

    // Restore vertex array.
    Buffer* buffer = attrib->buffer();
    GLuint buffer_service_id = buffer ? buffer->service_id() : 0;
    api()->glBindBufferFn(GL_ARRAY_BUFFER, buffer_service_id);
    const void* ptr = reinterpret_cast<const void*>(attrib->offset());
    api()->glVertexAttribPointerFn(attrib_index, attrib->size(), attrib->type(),
                                   attrib->normalized(), attrib->gl_stride(),
                                   ptr);

    // Restore attrib divisor if supported.
    if (feature_info_->feature_flags().angle_instanced_arrays)
      api()->glVertexAttribDivisorANGLEFn(attrib_index, attrib->divisor());

    if (attrib->enabled_in_driver()) {
      api()->glEnableVertexAttribArrayFn(attrib_index);
    } else {
      api()->glDisableVertexAttribArrayFn(attrib_index);
    }
  }
}

void ContextState::RestoreVertexAttribs(const ContextState* prev_state) const {
  // Restore Vertex Attrib Arrays
  DCHECK(vertex_attrib_manager.get());
  // Restore VAOs.
  if (feature_info_->feature_flags().native_vertex_array_object) {
    // If default VAO is still using shared id 0 instead of unique ids
    // per-context, default VAO state must be restored.
    GLuint default_vao_service_id = default_vertex_attrib_manager->service_id();
    if (default_vao_service_id == 0)
      RestoreVertexAttribArrays(default_vertex_attrib_manager);

    // Restore the current VAO binding, unless it's the same as the
    // default above.
    GLuint curr_vao_service_id = vertex_attrib_manager->service_id();
    if (curr_vao_service_id != 0)
      api()->glBindVertexArrayOESFn(curr_vao_service_id);
  } else {
    if (prev_state &&
        prev_state->feature_info_->feature_flags().native_vertex_array_object &&
        feature_info_->workarounds()
            .use_client_side_arrays_for_stream_buffers) {
      // In order to use client side arrays, the driver's default VAO has to be
      // bound.
      api()->glBindVertexArrayOESFn(0);
    }
    // If native VAO isn't supported, emulated VAOs are used.
    // Restore to the currently bound VAO.
    RestoreVertexAttribArrays(vertex_attrib_manager);
  }

  // glVertexAttrib4fv aren't part of VAO state and must be restored.
  RestoreVertexAttribValues();
}

void ContextState::RestoreGlobalState(const ContextState* prev_state) const {
  InitCapabilities(prev_state);
  InitState(prev_state);
}

void ContextState::RestoreState(const ContextState* prev_state) {
  RestoreAllTextureUnitAndSamplerBindings(prev_state);
  // For RasterDecoder, |vertex_attrib_manager| will be nullptr, and we don't
  // need restore vertex attribs for them.
  if (vertex_attrib_manager)
    RestoreVertexAttribs(prev_state);
  // RestoreIndexedUniformBufferBindings must be called before
  // RestoreBufferBindings. This is because setting the indexed uniform buffer
  // bindings via glBindBuffer{Base,Range} also sets the general uniform buffer
  // bindings (glBindBuffer), but not vice versa.
  // For RasterDecoder, |indexed_uniform_buffer_bindings| will be nullptr, and
  // we don't need restore indexed uniform buffer for them.
  if (indexed_uniform_buffer_bindings)
    RestoreIndexedUniformBufferBindings(prev_state);
  RestoreBufferBindings();
  RestoreRenderbufferBindings();
  RestoreProgramSettings(prev_state, true);
  RestoreGlobalState(prev_state);

  // FRAMEBUFFER_SRGB will be restored lazily at render time.
  framebuffer_srgb_valid_ = false;
}

void ContextState::EnableDisable(GLenum pname, bool enable) const {
  if (pname == GL_PRIMITIVE_RESTART_FIXED_INDEX &&
      feature_info_->feature_flags().emulate_primitive_restart_fixed_index) {
    // GLES2DecoderImpl::DoDrawElements can handle this situation
    return;
  }
  if (enable) {
    api()->glEnableFn(pname);
  } else {
    api()->glDisableFn(pname);
  }
}

void ContextState::UpdatePackParameters() const {
  if (!feature_info_->IsES3Capable())
    return;
  if (bound_pixel_pack_buffer.get()) {
    api()->glPixelStoreiFn(GL_PACK_ROW_LENGTH, pack_row_length);
  } else {
    api()->glPixelStoreiFn(GL_PACK_ROW_LENGTH, 0);
  }
}

void ContextState::SetMaxWindowRectangles(size_t max) {
  window_rectangles_ = std::vector<GLint>(max * 4, 0);
}

size_t ContextState::GetMaxWindowRectangles() const {
  size_t size = window_rectangles_.size();
  DCHECK_EQ(0ull, size % 4);
  return size / 4;
}

void ContextState::SetWindowRectangles(GLenum mode,
                                       size_t count,
                                       const volatile GLint* box) {
  window_rectangles_mode = mode;
  num_window_rectangles = count;
  DCHECK_LE(count, GetMaxWindowRectangles());
  if (count) {
    std::copy(box, &box[count * 4], window_rectangles_.begin());
  }
}

void ContextState::UpdateWindowRectangles() const {
  if (!feature_info_->feature_flags().ext_window_rectangles) {
    return;
  }

  if (current_draw_framebuffer_client_id == 0) {
    // Window rectangles must not take effect for client_id 0 (backbuffer).
    api()->glWindowRectanglesEXTFn(GL_EXCLUSIVE_EXT, 0, nullptr);
  } else {
    DCHECK_LE(static_cast<size_t>(num_window_rectangles),
              GetMaxWindowRectangles());
    const GLint* data =
        num_window_rectangles ? window_rectangles_.data() : nullptr;
    api()->glWindowRectanglesEXTFn(window_rectangles_mode,
                                   num_window_rectangles, data);
  }
}

void ContextState::UpdateWindowRectanglesForBoundDrawFramebufferClientID(
    GLuint client_id) {
  bool old_id_nonzero = current_draw_framebuffer_client_id != 0;
  bool new_id_nonzero = client_id != 0;
  current_draw_framebuffer_client_id = client_id;
  // If switching from FBO to backbuffer, or vice versa, update driver state.
  if (old_id_nonzero ^ new_id_nonzero) {
    UpdateWindowRectangles();
  }
}

void ContextState::UpdateUnpackParameters() const {
  if (!feature_info_->IsES3Capable())
    return;
  if (bound_pixel_unpack_buffer.get()) {
    api()->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, unpack_row_length);
    api()->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, unpack_image_height);
  } else {
    api()->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, 0);
    api()->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, 0);
  }
}

void ContextState::SetBoundBuffer(GLenum target, Buffer* buffer) {
  bool do_refcounting = feature_info_->IsWebGL2OrES3Context();
  switch (target) {
    case GL_ARRAY_BUFFER:
      if (do_refcounting && bound_array_buffer)
        bound_array_buffer->OnUnbind(target, false);
      bound_array_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      vertex_attrib_manager->SetElementArrayBuffer(buffer);
      break;
    case GL_COPY_READ_BUFFER:
      if (do_refcounting && bound_copy_read_buffer)
        bound_copy_read_buffer->OnUnbind(target, false);
      bound_copy_read_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      break;
    case GL_COPY_WRITE_BUFFER:
      if (do_refcounting && bound_copy_write_buffer)
        bound_copy_write_buffer->OnUnbind(target, false);
      bound_copy_write_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      break;
    case GL_PIXEL_PACK_BUFFER:
      if (do_refcounting && bound_pixel_pack_buffer)
        bound_pixel_pack_buffer->OnUnbind(target, false);
      bound_pixel_pack_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      UpdatePackParameters();
      break;
    case GL_PIXEL_UNPACK_BUFFER:
      if (do_refcounting && bound_pixel_unpack_buffer)
        bound_pixel_unpack_buffer->OnUnbind(target, false);
      bound_pixel_unpack_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      UpdateUnpackParameters();
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      if (do_refcounting && bound_transform_feedback_buffer)
        bound_transform_feedback_buffer->OnUnbind(target, false);
      bound_transform_feedback_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      break;
    case GL_UNIFORM_BUFFER:
      if (do_refcounting && bound_uniform_buffer)
        bound_uniform_buffer->OnUnbind(target, false);
      bound_uniform_buffer = buffer;
      if (do_refcounting && buffer)
        buffer->OnBind(target, false);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ContextState::RemoveBoundBuffer(Buffer* buffer) {
  DCHECK(buffer);
  bool do_refcounting = feature_info_->IsWebGL2OrES3Context();
  if (bound_array_buffer.get() == buffer) {
    bound_array_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_ARRAY_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_ARRAY_BUFFER, 0);
  }
  // Needs to be called after bound_array_buffer handled.
  vertex_attrib_manager->Unbind(buffer, bound_array_buffer.get());
  if (bound_copy_read_buffer.get() == buffer) {
    bound_copy_read_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_COPY_READ_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_COPY_READ_BUFFER, 0);
  }
  if (bound_copy_write_buffer.get() == buffer) {
    bound_copy_write_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_COPY_WRITE_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_COPY_WRITE_BUFFER, 0);
  }
  if (bound_pixel_pack_buffer.get() == buffer) {
    bound_pixel_pack_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_PIXEL_PACK_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER, 0);
    UpdatePackParameters();
  }
  if (bound_pixel_unpack_buffer.get() == buffer) {
    bound_pixel_unpack_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_PIXEL_UNPACK_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
    UpdateUnpackParameters();
  }
  if (bound_transform_feedback_buffer.get() == buffer) {
    bound_transform_feedback_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_TRANSFORM_FEEDBACK_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
  }
  // Needs to be called after bound_transform_feedback_buffer handled.
  if (bound_transform_feedback.get()) {
    bound_transform_feedback->RemoveBoundBuffer(
        GL_TRANSFORM_FEEDBACK_BUFFER, buffer,
        bound_transform_feedback_buffer.get(), !context_lost_);
  }
  if (bound_uniform_buffer.get() == buffer) {
    bound_uniform_buffer = nullptr;
    if (do_refcounting)
      buffer->OnUnbind(GL_UNIFORM_BUFFER, false);
    if (!context_lost_)
      api()->glBindBufferFn(GL_UNIFORM_BUFFER, 0);
  }
  // Needs to be called after bound_uniform_buffer handled.
  if (indexed_uniform_buffer_bindings) {
    indexed_uniform_buffer_bindings->RemoveBoundBuffer(
        GL_UNIFORM_BUFFER, buffer, bound_uniform_buffer.get(), !context_lost_);
  }
}

void ContextState::UnbindTexture(TextureRef* texture) {
  GLuint active_unit = active_texture_unit;
  for (size_t jj = 0; jj < texture_units.size(); ++jj) {
    TextureUnit& unit = texture_units[jj];
    if (unit.bound_texture_2d.get() == texture) {
      unit.bound_texture_2d = nullptr;
      if (active_unit != jj) {
        api()->glActiveTextureFn(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      api()->glBindTextureFn(GL_TEXTURE_2D, 0);
    } else if (unit.bound_texture_cube_map.get() == texture) {
      unit.bound_texture_cube_map = nullptr;
      if (active_unit != jj) {
        api()->glActiveTextureFn(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      api()->glBindTextureFn(GL_TEXTURE_CUBE_MAP, 0);
    } else if (unit.bound_texture_external_oes.get() == texture) {
      unit.bound_texture_external_oes = nullptr;
      if (active_unit != jj) {
        api()->glActiveTextureFn(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      api()->glBindTextureFn(GL_TEXTURE_EXTERNAL_OES, 0);
    } else if (unit.bound_texture_rectangle_arb.get() == texture) {
      unit.bound_texture_rectangle_arb = nullptr;
      if (active_unit != jj) {
        api()->glActiveTextureFn(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      api()->glBindTextureFn(GL_TEXTURE_RECTANGLE_ARB, 0);
    } else if (unit.bound_texture_3d.get() == texture) {
      unit.bound_texture_3d = nullptr;
      if (active_unit != jj) {
        api()->glActiveTextureFn(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      api()->glBindTextureFn(GL_TEXTURE_3D, 0);
    } else if (unit.bound_texture_2d_array.get() == texture) {
      unit.bound_texture_2d_array = nullptr;
      if (active_unit != jj) {
        api()->glActiveTextureFn(GL_TEXTURE0 + jj);
        active_unit = jj;
      }
      api()->glBindTextureFn(GL_TEXTURE_2D_ARRAY, 0);
    }
  }

  if (active_unit != active_texture_unit) {
    api()->glActiveTextureFn(GL_TEXTURE0 + active_texture_unit);
  }
}

void ContextState::UnbindSampler(Sampler* sampler) {
  for (size_t jj = 0; jj < sampler_units.size(); ++jj) {
    if (sampler_units[jj].get() == sampler) {
      sampler_units[jj] = nullptr;
      api()->glBindSamplerFn(jj, 0);
    }
  }
}

PixelStoreParams ContextState::GetPackParams() {
  DCHECK_EQ(0, pack_skip_pixels);
  DCHECK_EQ(0, pack_skip_rows);
  PixelStoreParams params;
  params.alignment = pack_alignment;
  params.row_length = pack_row_length;
  return params;
}

PixelStoreParams ContextState::GetUnpackParams(Dimension dimension) {
  DCHECK_EQ(0, unpack_skip_pixels);
  DCHECK_EQ(0, unpack_skip_rows);
  DCHECK_EQ(0, unpack_skip_images);
  PixelStoreParams params;
  params.alignment = unpack_alignment;
  params.row_length = unpack_row_length;
  if (dimension == k3D) {
    params.image_height = unpack_image_height;
  }
  return params;
}

void ContextState::EnableDisableFramebufferSRGB(bool enable) {
  if (framebuffer_srgb_valid_ && framebuffer_srgb_ == enable)
    return;
  EnableDisable(GL_FRAMEBUFFER_SRGB, enable);
  framebuffer_srgb_ = enable;
  framebuffer_srgb_valid_ = true;
}

void ContextState::InitStateManual(const ContextState*) const {
  // Here we always reset the states whether it's different from previous ones.
  // We have very limited states here; also, once we switch to MANGLE, MANGLE
  // will opmitize this.
  UpdatePackParameters();
  UpdateUnpackParameters();
  UpdateWindowRectangles();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu
