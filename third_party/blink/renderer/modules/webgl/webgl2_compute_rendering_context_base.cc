// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl2_compute_rendering_context_base.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/modules/webgl/webgl_program.h"
#include "third_party/blink/renderer/modules/webgl/webgl_uniform_location.h"
#include "third_party/blink/renderer/modules/webgl/webgl_vertex_array_object.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebGL2ComputeRenderingContextBase::WebGL2ComputeRenderingContextBase(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGL2RenderingContextBase(host,
                                 std::move(context_provider),
                                 using_gpu_compositing,
                                 requested_attributes,
                                 Platform::kWebGL2ComputeContextType) {}

void WebGL2ComputeRenderingContextBase::DestroyContext() {
  WebGL2RenderingContextBase::DestroyContext();
}

void WebGL2ComputeRenderingContextBase::InitializeNewContext() {
  DCHECK(!isContextLost());
  DCHECK(GetDrawingBuffer());

  bound_dispatch_indirect_buffer_ = nullptr;
  bound_draw_indirect_buffer_ = nullptr;
  bound_atomic_counter_buffer_ = nullptr;
  bound_shader_storage_buffer_ = nullptr;

  GLint max_atomic_counter_buffer_bindings = 0;
  ContextGL()->GetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,
                           &max_atomic_counter_buffer_bindings);
  bound_indexed_atomic_counter_buffers_.clear();
  bound_indexed_atomic_counter_buffers_.resize(
      max_atomic_counter_buffer_bindings);

  GLint max_shader_storage_buffer_bindings = 0;
  ContextGL()->GetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
                           &max_shader_storage_buffer_bindings);
  bound_indexed_shader_storage_buffers_.clear();
  bound_indexed_shader_storage_buffers_.resize(
      max_shader_storage_buffer_bindings);

  WebGL2RenderingContextBase::InitializeNewContext();
}

void WebGL2ComputeRenderingContextBase::dispatchCompute(GLuint numGroupsX,
                                                        GLuint numGroupsY,
                                                        GLuint numGroupsZ) {
  ContextGL()->DispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void WebGL2ComputeRenderingContextBase::dispatchComputeIndirect(
    int64_t offset) {
  if (!ValidateValueFitNonNegInt32("dispatchComputeIndirect", "offset", offset))
    return;
  ContextGL()->DispatchComputeIndirect(static_cast<GLintptr>(offset));
}

void WebGL2ComputeRenderingContextBase::drawArraysIndirect(
    GLenum mode,
    int64_t offset) {
  if (!ValidateValueFitNonNegInt32("drawArraysIndirect", "offset", offset))
    return;

  if (!ValidateDrawArrays("drawArraysIndirect"))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawArraysIndirect",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.get());
  OnBeforeDrawCall();
  ContextGL()->DrawArraysIndirect(
      mode, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void WebGL2ComputeRenderingContextBase::drawElementsIndirect(
    GLenum mode,
    GLenum type,
    int64_t offset) {
  if (!ValidateValueFitNonNegInt32("drawElementsIndirect", "offset", offset))
    return;

  // The buffer currently bound to the (GL_)DRAW_INDIRECT_BUFFER binding might
  // be unpopulated at this point, so the validation of element array buffer
  // offset in it needs to be deferred. By feeding a dummy in-range offset value
  // here, other validation logic for indexed drawing can be reused.
  int64_t dummy_offset = 0;
  if (!ValidateDrawElements("drawElementsIndirect", type, dummy_offset))
    return;

  if (!bound_vertex_array_object_->IsAllEnabledAttribBufferBound()) {
    SynthesizeGLError(GL_INVALID_OPERATION, "drawElementsIndirect",
                      "no buffer is bound to enabled attribute");
    return;
  }

  ScopedRGBEmulationColorMask emulation_color_mask(this, color_mask_,
                                                   drawing_buffer_.get());
  OnBeforeDrawCall();
  ContextGL()->DrawElementsIndirect(
      mode, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

ScriptValue WebGL2ComputeRenderingContextBase::getProgramInterfaceParameter(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum program_interface,
    GLenum pname) {
  if (!ValidateWebGLProgramOrShader("getProgramInterfaceParameter", program))
    return ScriptValue::CreateNull(script_state->GetIsolate());
  if (!ValidateProgramInterface(
      "getProgramInterfaceParameter", program_interface))
    return ScriptValue::CreateNull(script_state->GetIsolate());
  if (program_interface == GL_ATOMIC_COUNTER_BUFFER &&
      pname == GL_MAX_NAME_LENGTH) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getProgramInterfaceParameter",
                      "atomic counter resources are not assigned name strings");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }
  if (program_interface != GL_ATOMIC_COUNTER_BUFFER &&
      program_interface != GL_SHADER_STORAGE_BLOCK &&
      program_interface != GL_UNIFORM_BLOCK &&
      pname == GL_MAX_NUM_ACTIVE_VARIABLES) {
    SynthesizeGLError(
        GL_INVALID_OPERATION, "getProgramInterfaceParameter",
        "invalid parameter name for the specified program interface");
    return ScriptValue::CreateNull(script_state->GetIsolate());
  }

  switch (pname) {
    case GL_ACTIVE_RESOURCES:
    case GL_MAX_NAME_LENGTH:
    case GL_MAX_NUM_ACTIVE_VARIABLES: {
      GLint value = 0;
      ContextGL()->GetProgramInterfaceiv(
          ObjectOrZero(program), program_interface, pname, &value);
      return WebGLAny(script_state, value);
    }
    default:
      SynthesizeGLError(GL_INVALID_ENUM, "getProgramInterfaceParameter",
                        "invalid parameter name");
      return ScriptValue::CreateNull(script_state->GetIsolate());
  }
}

GLuint WebGL2ComputeRenderingContextBase::getProgramResourceIndex(
    WebGLProgram* program,
    GLenum program_interface,
    const String& name) {
  if (!ValidateWebGLProgramOrShader("getProgramResourceIndex", program))
    return GL_INVALID_INDEX;
  if (!ValidateProgramInterface("getProgramResourceIndex", program_interface))
    return GL_INVALID_INDEX;
  if (program_interface == GL_ATOMIC_COUNTER_BUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "getProgramResourceIndex",
                      "atomic counter resources are not assigned name strings");
    return GL_INVALID_INDEX;
  }
  if (!ValidateString("getProgramResourceIndex", name))
    return GL_INVALID_INDEX;

  return ContextGL()->GetProgramResourceIndex(
      ObjectOrZero(program), program_interface, name.Utf8().c_str());
}

String WebGL2ComputeRenderingContextBase::getProgramResourceName(
    WebGLProgram* program,
    GLenum program_interface,
    GLuint index) {
  if (!ValidateWebGLProgramOrShader("getProgramResourceName", program))
    return String();
  if (!ValidateProgramInterface("getProgramResourceName", program_interface))
    return String();
  if (program_interface == GL_ATOMIC_COUNTER_BUFFER) {
    SynthesizeGLError(GL_INVALID_ENUM, "getProgramResourceName",
                      "atomic counter resources are not assigned name strings");
    return String();
  }
  if (!ValidateProgramResourceIndex(
      "getProgramResourceName", program, program_interface, index))
    return String();

  GLint max_name_length = -1;
  ContextGL()->GetProgramInterfaceiv(ObjectOrZero(program), program_interface,
                                     GL_MAX_NAME_LENGTH, &max_name_length);
  if (max_name_length <= 0)
    return String();
  auto name = std::make_unique<GLchar[]>(max_name_length);

  GLsizei length = 0;
  ContextGL()->GetProgramResourceName(ObjectOrZero(program), program_interface,
                                      index, max_name_length, &length,
                                      name.get());
  if (length <= 0)
    return String();
  return String(name.get(), static_cast<uint32_t>(length));
}

base::Optional<HeapVector<ScriptValue>>
WebGL2ComputeRenderingContextBase::getProgramResource(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum program_interface,
    GLuint index,
    const Vector<GLenum>& props) {
  if (!ValidateWebGLProgramOrShader("getProgramResource", program))
    return base::nullopt;
  if (!ValidateProgramInterface("getProgramResource", program_interface))
    return base::nullopt;
  if (props.IsEmpty()) {
    SynthesizeGLError(GL_INVALID_VALUE, "getProgramResource",
                      "resource prop array is empty");
    return base::nullopt;
  }
  if (!ValidateProgramResourceIndex(
      "getProgramResource", program, program_interface, index))
    return base::nullopt;

  // For props with variable-length return values, their lengths will be queried
  // first with |auxiliary_props|, and |extended_params| will be adequately
  // sized for the whole result after that.

  Vector<GLenum> auxiliary_props;
  Vector<GLint> auxiliary_params;
  Vector<GLenum> extended_props;
  Vector<GLint> extended_params;
  if (!ValidateAndExtendProgramResourceProperties(
      "getProgramResource", program_interface, props, extended_props))
    return base::nullopt;
  extended_params.resize(extended_props.size());
  for (wtf_size_t i = 0; i < extended_props.size() - props.size(); ++i) {
    auxiliary_props.push_back(extended_props[i]);
    auxiliary_params.push_back(-1);
    extended_params.pop_back();
  }

  if (auxiliary_props.size()) {
    ContextGL()->GetProgramResourceiv(ObjectOrZero(program), program_interface,
                                      index, auxiliary_props.size(),
                                      auxiliary_props.data(),
                                      auxiliary_params.size(), nullptr,
                                      auxiliary_params.data());
    for (GLint n : auxiliary_params) {
      extended_params.resize(extended_params.size() + std::max(n, 0));
    }
  }

  GLsizei length = 0;
  ContextGL()->GetProgramResourceiv(ObjectOrZero(program), program_interface,
                                    index, extended_props.size(),
                                    extended_props.data(),
                                    extended_params.size(), &length,
                                    extended_params.data());
  if (length <= 0) {
    return base::nullopt;
  }
  for (wtf_size_t i = 0; i < auxiliary_params.size(); ++i) {
    // The returned lengths really should not differ from the previous ones.
    CHECK_EQ(extended_params[i], auxiliary_params[i]);
  }

  // Interpret the returned values and construct the result array. The type of
  // each array element is the natural type for the requested property.
  HeapVector<ScriptValue> result;
  wtf_size_t auxiliary_param_index = 0;
  wtf_size_t extended_param_index = auxiliary_params.size();
  for (GLenum prop : props) {
    switch (prop) {
      case GL_IS_ROW_MAJOR:
      case GL_REFERENCED_BY_COMPUTE_SHADER:
      case GL_REFERENCED_BY_FRAGMENT_SHADER:
      case GL_REFERENCED_BY_VERTEX_SHADER: {
        bool value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_ARRAY_STRIDE:
      case GL_ATOMIC_COUNTER_BUFFER_INDEX:
      case GL_BLOCK_INDEX:
      case GL_MATRIX_STRIDE:
      case GL_OFFSET: {
        int value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_LOCATION: {
        int value = extended_params[extended_param_index];
        result.push_back(
            WrapLocation(script_state, value, program, program_interface));
        ++extended_param_index;
        break;
      }
      case GL_ARRAY_SIZE:
      case GL_BUFFER_BINDING:
      case GL_BUFFER_DATA_SIZE:
      case GL_NAME_LENGTH:
      case GL_NUM_ACTIVE_VARIABLES:
      case GL_TOP_LEVEL_ARRAY_SIZE:
      case GL_TOP_LEVEL_ARRAY_STRIDE: {
        unsigned value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_TYPE: {
        GLenum value = extended_params[extended_param_index];
        result.push_back(WebGLAny(script_state, value));
        ++extended_param_index;
        break;
      }
      case GL_ACTIVE_VARIABLES: {
        DOMUint32Array* value = DOMUint32Array::Create(
            reinterpret_cast<unsigned*>(&extended_params[extended_param_index]),
            auxiliary_params[auxiliary_param_index]);
        result.push_back(WebGLAny(script_state, value));
        extended_param_index += auxiliary_params[auxiliary_param_index];
        ++auxiliary_param_index;
        break;
      }
      default:
        NOTREACHED();
    }
  }

  return result;
}

ScriptValue WebGL2ComputeRenderingContextBase::getProgramResourceLocation(
    ScriptState* script_state,
    WebGLProgram* program,
    GLenum program_interface,
    const String& name) {
  if (!ValidateWebGLProgramOrShader("getProgramResourceLocation", program))
    return WrapLocation(script_state, -1, program, program_interface);
  if (!ValidateProgramInterface(
      "getProgramResourceLocation", program_interface))
    return WrapLocation(script_state, -1, program, program_interface);
  if (!ValidateLocationLength("getProgramResourceLocation", name))
    return WrapLocation(script_state, -1, program, program_interface);
  if (!ValidateString("getProgramResourceLocation", name))
    return WrapLocation(script_state, -1, program, program_interface);
  if (IsPrefixReserved(name))
    return WrapLocation(script_state, -1, program, program_interface);
  if (!program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, "getProgramResourceLocation",
                      "program not linked");
    return WrapLocation(script_state, -1, program, program_interface);
  }

  GLint location = ContextGL()->GetProgramResourceLocation(
      ObjectOrZero(program), program_interface, name.Utf8().c_str());
  return WrapLocation(script_state, location, program, program_interface);
}

void WebGL2ComputeRenderingContextBase::bindImageTexture(GLuint unit,
                                                         WebGLTexture* texture,
                                                         GLint level,
                                                         GLboolean layered,
                                                         GLint layer,
                                                         GLenum access,
                                                         GLenum format) {
  ContextGL()->BindImageTexture(unit, ObjectOrZero(texture), level, layered,
                                layer, access, format);
}

void WebGL2ComputeRenderingContextBase::memoryBarrier(GLbitfield barriers) {
  ContextGL()->MemoryBarrierEXT(barriers);
}

void WebGL2ComputeRenderingContextBase::memoryBarrierByRegion(
    GLbitfield barriers) {
  ContextGL()->MemoryBarrierByRegion(barriers);
}

ScriptValue WebGL2ComputeRenderingContextBase::getParameter(
    ScriptState* script_state,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());
  switch (pname) {
    case GL_SHADING_LANGUAGE_VERSION: {
      return WebGLAny(
          script_state,
          "WebGL GLSL ES 3.10 (" +
              String(ContextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) +
              ")");
    }
    case GL_VERSION: {
      return WebGLAny(script_state,
                      "WebGL 2.0 Compute (" +
                          String(ContextGL()->GetString(GL_VERSION)) + ")");
    }
    case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
    case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
    case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_COMBINED_ATOMIC_COUNTERS:
    case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
    case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
    case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
    case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
    case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
    case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
    case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
    case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
    case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_VERTEX_ATOMIC_COUNTERS:
    case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
      return GetInt64Parameter(script_state, pname);
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
      return WebGLAny(script_state, bound_dispatch_indirect_buffer_.Get());
    case GL_DRAW_INDIRECT_BUFFER_BINDING:
      return WebGLAny(script_state, bound_draw_indirect_buffer_.Get());

    default:
      return WebGL2RenderingContextBase::getParameter(script_state, pname);
  }
}

ScriptValue WebGL2ComputeRenderingContextBase::getIndexedParameter(
    ScriptState* script_state,
    GLenum target,
    GLuint index) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  switch (target) {
    case GL_ATOMIC_COUNTER_BUFFER_BINDING:
      if (index >= bound_indexed_atomic_counter_buffers_.size()) {
        SynthesizeGLError(GL_INVALID_VALUE, "getIndexedParameter",
                          "index out of range");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      return WebGLAny(script_state,
                      bound_indexed_atomic_counter_buffers_[index].Get());
    case GL_SHADER_STORAGE_BUFFER_BINDING:
      if (index >= bound_indexed_shader_storage_buffers_.size()) {
        SynthesizeGLError(GL_INVALID_VALUE, "getIndexedParameter",
                          "index out of range");
        return ScriptValue::CreateNull(script_state->GetIsolate());
      }
      return WebGLAny(script_state,
                      bound_indexed_shader_storage_buffers_[index].Get());
    case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
    case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
    case GL_ATOMIC_COUNTER_BUFFER_SIZE:
    case GL_ATOMIC_COUNTER_BUFFER_START:
    case GL_SHADER_STORAGE_BUFFER_SIZE:
    case GL_SHADER_STORAGE_BUFFER_START: {
      GLint64 value = -1;
      ContextGL()->GetInteger64i_v(target, index, &value);
      return WebGLAny(script_state, value);
    }
    default:
      return WebGL2RenderingContextBase::getIndexedParameter(
          script_state, target, index);
  }
}

void WebGL2ComputeRenderingContextBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(bound_dispatch_indirect_buffer_);
  visitor->Trace(bound_draw_indirect_buffer_);
  visitor->Trace(bound_atomic_counter_buffer_);
  visitor->Trace(bound_indexed_atomic_counter_buffers_);
  visitor->Trace(bound_shader_storage_buffer_);
  visitor->Trace(bound_indexed_shader_storage_buffers_);
  WebGL2RenderingContextBase::Trace(visitor);
}

bool WebGL2ComputeRenderingContextBase::ValidateProgramInterface(
    const char* function_name,
    GLenum program_interface) {
  switch (program_interface) {
    case GL_ATOMIC_COUNTER_BUFFER:
    case GL_BUFFER_VARIABLE:
    case GL_PROGRAM_INPUT:
    case GL_PROGRAM_OUTPUT:
    case GL_SHADER_STORAGE_BLOCK:
    case GL_TRANSFORM_FEEDBACK_VARYING:
    case GL_UNIFORM:
    case GL_UNIFORM_BLOCK:
      return true;
    default:
      SynthesizeGLError(GL_INVALID_ENUM, function_name,
                        "invalid program interface");
      return false;
  }
}

bool WebGL2ComputeRenderingContextBase::ValidateProgramResourceIndex(
    const char* function_name,
    WebGLProgram* program,
    GLenum program_interface,
    GLuint index) {
  DCHECK(program);
  if (!program->LinkStatus(this)) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                      "program not linked");
    return false;
  }
  GLint active_resources = 0;
  ContextGL()->GetProgramInterfaceiv(ObjectOrZero(program), program_interface,
                                     GL_ACTIVE_RESOURCES, &active_resources);
  if (index >= static_cast<GLuint>(active_resources)) {
    SynthesizeGLError(GL_INVALID_VALUE, function_name,
                      "invalid program resource index");
    return false;
  }
  return true;
}

bool
WebGL2ComputeRenderingContextBase::ValidateAndExtendProgramResourceProperties(
    const char* function_name,
    GLenum program_interface,
    const Vector<GLenum>& props,
    Vector<GLenum>& extended_props) {
  Vector<GLenum> auxiliary_props;

  for (GLenum prop : props) {
    GLenum error = GL_NO_ERROR;

    switch (prop) {
      // Handle props with fixed-length return values.
      case GL_BUFFER_BINDING:
      case GL_NUM_ACTIVE_VARIABLES:
        if (program_interface != GL_ATOMIC_COUNTER_BUFFER &&
            program_interface != GL_SHADER_STORAGE_BLOCK &&
            program_interface != GL_UNIFORM_BLOCK)
          error = GL_INVALID_OPERATION;
        break;
      case GL_ARRAY_SIZE:
        if (program_interface != GL_BUFFER_VARIABLE &&
            program_interface != GL_PROGRAM_INPUT &&
            program_interface != GL_PROGRAM_OUTPUT &&
            program_interface != GL_TRANSFORM_FEEDBACK_VARYING &&
            program_interface != GL_UNIFORM)
          error = GL_INVALID_OPERATION;
        break;
      case GL_ARRAY_STRIDE:
      case GL_BLOCK_INDEX:
      case GL_IS_ROW_MAJOR:
      case GL_MATRIX_STRIDE:
        if (program_interface != GL_BUFFER_VARIABLE &&
            program_interface != GL_UNIFORM)
          error = GL_INVALID_OPERATION;
        break;
      case GL_ATOMIC_COUNTER_BUFFER_INDEX:
        if (program_interface != GL_UNIFORM)
          error = GL_INVALID_OPERATION;
        break;
      case GL_BUFFER_DATA_SIZE:
        if (program_interface != GL_ATOMIC_COUNTER_BUFFER &&
            program_interface != GL_SHADER_STORAGE_BLOCK &&
            program_interface != GL_UNIFORM_BLOCK)
          error = GL_INVALID_OPERATION;
        break;
      case GL_LOCATION:
        if (program_interface != GL_PROGRAM_INPUT &&
            program_interface != GL_PROGRAM_OUTPUT &&
            program_interface != GL_UNIFORM)
          error = GL_INVALID_OPERATION;
        break;
      case GL_NAME_LENGTH:
        if (program_interface == GL_ATOMIC_COUNTER_BUFFER)
          error = GL_INVALID_OPERATION;
        break;
      case GL_OFFSET:
        if (program_interface != GL_BUFFER_VARIABLE &&
            program_interface != GL_UNIFORM)
          error = GL_INVALID_OPERATION;
        break;
      case GL_REFERENCED_BY_VERTEX_SHADER:
      case GL_REFERENCED_BY_FRAGMENT_SHADER:
      case GL_REFERENCED_BY_COMPUTE_SHADER:
        if (program_interface != GL_ATOMIC_COUNTER_BUFFER &&
            program_interface != GL_BUFFER_VARIABLE &&
            program_interface != GL_PROGRAM_INPUT &&
            program_interface != GL_PROGRAM_OUTPUT &&
            program_interface != GL_SHADER_STORAGE_BLOCK &&
            program_interface != GL_UNIFORM &&
            program_interface != GL_UNIFORM_BLOCK)
          error = GL_INVALID_OPERATION;
        break;
      case GL_TOP_LEVEL_ARRAY_SIZE:
      case GL_TOP_LEVEL_ARRAY_STRIDE:
        if (program_interface != GL_BUFFER_VARIABLE)
          error = GL_INVALID_OPERATION;
        break;
      case GL_TYPE:
        if (program_interface != GL_BUFFER_VARIABLE &&
            program_interface != GL_PROGRAM_INPUT &&
            program_interface != GL_PROGRAM_OUTPUT &&
            program_interface != GL_TRANSFORM_FEEDBACK_VARYING &&
            program_interface != GL_UNIFORM)
          error = GL_INVALID_OPERATION;
        break;

      // Handle props with variable-length return values.
      case GL_ACTIVE_VARIABLES:
        if (program_interface != GL_ATOMIC_COUNTER_BUFFER &&
            program_interface != GL_SHADER_STORAGE_BLOCK &&
            program_interface != GL_UNIFORM_BLOCK) {
          error = GL_INVALID_OPERATION;
          break;
        }
        auxiliary_props.push_back(GL_NUM_ACTIVE_VARIABLES);
        break;

      default:
        error = GL_INVALID_ENUM;
    }

    switch (error) {
      case GL_NO_ERROR:
        break;
      case GL_INVALID_ENUM:
        SynthesizeGLError(GL_INVALID_ENUM, function_name,
                          "invalid program resource prop");
        return false;
      case GL_INVALID_OPERATION:
        SynthesizeGLError(
            GL_INVALID_OPERATION, function_name,
            "invalid resource prop for the specified program interface");
        return false;
      default:
        NOTREACHED();
    }
  }

  extended_props = auxiliary_props;
  extended_props.AppendVector(props);
  return true;
}

ScriptValue WebGL2ComputeRenderingContextBase::WrapLocation(
    ScriptState* script_state,
    GLint location,
    WebGLProgram* program,
    GLenum program_interface) {
  switch (program_interface) {
    case GL_PROGRAM_INPUT:
    case GL_PROGRAM_OUTPUT: {
      return WebGLAny(script_state, location);
    }
    case GL_UNIFORM: {
      if (location == -1)
        return ScriptValue::CreateNull(script_state->GetIsolate());
      DCHECK_GE(location, 0);
      WebGLUniformLocation* uniform_location =
          WebGLUniformLocation::Create(program, location);
      return ScriptValue(script_state->GetIsolate(),
                         ToV8(uniform_location, script_state));
    }
    default: {
      return WebGLAny(script_state, location);
    }
  }
}

bool WebGL2ComputeRenderingContextBase::ValidateShaderType(
    const char* function_name,
    GLenum shader_type) {
  switch (shader_type) {
    case GL_COMPUTE_SHADER:
      return true;
    default:
      return WebGL2RenderingContextBase::ValidateShaderType(
          function_name, shader_type);
  }
}

bool WebGL2ComputeRenderingContextBase::ValidateBufferTarget(
    const char* function_name,
    GLenum target) {
  switch (target) {
    case GL_DISPATCH_INDIRECT_BUFFER:
    case GL_DRAW_INDIRECT_BUFFER:
    case GL_ATOMIC_COUNTER_BUFFER:
    case GL_SHADER_STORAGE_BUFFER:
      return true;
    default:
      return WebGL2RenderingContextBase::ValidateBufferTarget(
          function_name, target);
  }
}

WebGLBuffer* WebGL2ComputeRenderingContextBase::ValidateBufferDataTarget(
    const char* function_name,
    GLenum target) {
  WebGLBuffer* buffer = nullptr;
  switch (target) {
    case GL_DISPATCH_INDIRECT_BUFFER:
      buffer = bound_dispatch_indirect_buffer_.Get();
      break;
    case GL_DRAW_INDIRECT_BUFFER:
      buffer = bound_draw_indirect_buffer_.Get();
      break;
    case GL_ATOMIC_COUNTER_BUFFER:
      buffer = bound_atomic_counter_buffer_.Get();
      break;
    case GL_SHADER_STORAGE_BUFFER:
      buffer = bound_shader_storage_buffer_.Get();
      break;
    default:
      return WebGL2RenderingContextBase::ValidateBufferDataTarget(
          function_name, target);
  }
  if (!buffer) {
    SynthesizeGLError(GL_INVALID_OPERATION, function_name, "no buffer");
    return nullptr;
  }
  return buffer;
}

bool WebGL2ComputeRenderingContextBase::ValidateAndUpdateBufferBindTarget(
    const char* function_name,
    GLenum target,
    WebGLBuffer* buffer) {
  if (!ValidateBufferTarget(function_name, target))
    return false;

  if (buffer &&
      !ValidateBufferTargetCompatibility(function_name, target, buffer))
    return false;

  switch (target) {
    case GL_DISPATCH_INDIRECT_BUFFER:
      bound_dispatch_indirect_buffer_ = buffer;
      break;
    case GL_DRAW_INDIRECT_BUFFER:
      bound_draw_indirect_buffer_ = buffer;
      break;
    case GL_ATOMIC_COUNTER_BUFFER:
      bound_atomic_counter_buffer_ = buffer;
      break;
    case GL_SHADER_STORAGE_BUFFER:
      bound_shader_storage_buffer_ = buffer;
      break;
    default:
      return WebGL2RenderingContextBase::ValidateAndUpdateBufferBindTarget(
          function_name, target, buffer);
  }

  if (buffer && !buffer->GetInitialTarget())
    buffer->SetInitialTarget(target);
  return true;
}

void WebGL2ComputeRenderingContextBase::RemoveBoundBuffer(WebGLBuffer* buffer) {
  if (bound_dispatch_indirect_buffer_ == buffer)
    bound_dispatch_indirect_buffer_ = nullptr;
  if (bound_draw_indirect_buffer_ == buffer)
    bound_draw_indirect_buffer_ = nullptr;
  if (bound_atomic_counter_buffer_ == buffer)
    bound_atomic_counter_buffer_ = nullptr;
  if (bound_shader_storage_buffer_ == buffer)
    bound_shader_storage_buffer_ = nullptr;

  WebGL2RenderingContextBase::RemoveBoundBuffer(buffer);
}

bool WebGL2ComputeRenderingContextBase::ValidateBufferTargetCompatibility(
    const char* function_name,
    GLenum target,
    WebGLBuffer* buffer) {
  DCHECK(buffer);

  switch (buffer->GetInitialTarget()) {
    case GL_ELEMENT_ARRAY_BUFFER:
      switch (target) {
        case GL_DISPATCH_INDIRECT_BUFFER:
        case GL_DRAW_INDIRECT_BUFFER:
        case GL_ATOMIC_COUNTER_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
          SynthesizeGLError(
              GL_INVALID_OPERATION, function_name,
              "element array buffers can not be bound to a different target");

          return false;
        default:
          break;
      }
      break;
    case GL_DISPATCH_INDIRECT_BUFFER:
    case GL_DRAW_INDIRECT_BUFFER:
    case GL_ATOMIC_COUNTER_BUFFER:
    case GL_SHADER_STORAGE_BUFFER:
      if (target == GL_ELEMENT_ARRAY_BUFFER) {
        SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                          "buffers bound to non ELEMENT_ARRAY_BUFFER targets "
                          "can not be bound to ELEMENT_ARRAY_BUFFER target");
        return false;
      }
      return true;
    default:
      break;
  }

  return WebGL2RenderingContextBase::ValidateBufferTargetCompatibility(
      function_name, target, buffer);
}

bool WebGL2ComputeRenderingContextBase::ValidateBufferBaseTarget(
    const char* function_name,
    GLenum target) {
  switch (target) {
    case GL_ATOMIC_COUNTER_BUFFER:
    case GL_SHADER_STORAGE_BUFFER:
      return true;
    default:
      return WebGL2RenderingContextBase::ValidateBufferBaseTarget(
          function_name, target);
  }
}

bool WebGL2ComputeRenderingContextBase::ValidateAndUpdateBufferBindBaseTarget(
    const char* function_name,
    GLenum target,
    GLuint index,
    WebGLBuffer* buffer) {
  if (!ValidateBufferBaseTarget(function_name, target))
    return false;

  if (buffer &&
      !ValidateBufferTargetCompatibility(function_name, target, buffer))
    return false;

  switch (target) {
    case GL_ATOMIC_COUNTER_BUFFER:
      if (index >= bound_indexed_atomic_counter_buffers_.size()) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "index out of range");
        return false;
      }
      bound_indexed_atomic_counter_buffers_[index] = buffer;
      bound_atomic_counter_buffer_ = buffer;
      break;
    case GL_SHADER_STORAGE_BUFFER:
      if (index >= bound_indexed_shader_storage_buffers_.size()) {
        SynthesizeGLError(GL_INVALID_VALUE, function_name,
                          "index out of range");
        return false;
      }
      bound_indexed_shader_storage_buffers_[index] = buffer;
      bound_shader_storage_buffer_ = buffer;
      break;
    default:
      return WebGL2RenderingContextBase::ValidateAndUpdateBufferBindBaseTarget(
          function_name, target, index, buffer);
  }

  if (buffer && !buffer->GetInitialTarget())
    buffer->SetInitialTarget(target);
  return true;
}

}  // namespace blink
