// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_COMPUTE_RENDERING_CONTEXT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_COMPUTE_RENDERING_CONTEXT_BASE_H_

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

class WebGLProgram;
class WebGLTexture;

class WebGL2ComputeRenderingContextBase : public WebGL2RenderingContextBase {
 public:
  void DestroyContext() override;

  /* Launch one or more compute work groups */
  void dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ);
  void dispatchComputeIndirect(int64_t offset);

  /* Draw indirect */
  void drawArraysIndirect(GLenum mode, int64_t offset);
  void drawElementsIndirect(GLenum mode, GLenum type, int64_t offset);

  /* Program interface query */
  ScriptValue getProgramInterfaceParameter(ScriptState*,
                                           WebGLProgram*,
                                           GLenum program_interface,
                                           GLenum pname);
  GLuint getProgramResourceIndex(WebGLProgram*,
                                 GLenum program_interface,
                                 const String& name);
  String getProgramResourceName(WebGLProgram*,
                                GLenum program_interface,
                                GLuint index);
  base::Optional<HeapVector<ScriptValue>> getProgramResource(
      ScriptState*,
      WebGLProgram*,
      GLenum program_interface,
      GLuint index,
      const Vector<GLenum>& props);
  ScriptValue getProgramResourceLocation(ScriptState*,
                                         WebGLProgram*,
                                         GLenum program_interface,
                                         const String& name);

  /* Bind a level of a texture to an image unit */
  void bindImageTexture(GLuint unit,
                        WebGLTexture* texture,
                        GLint level,
                        GLboolean layered,
                        GLint layer,
                        GLenum access,
                        GLenum format);

  /* Memory access synchronization */
  void memoryBarrier(GLbitfield barriers);
  void memoryBarrierByRegion(GLbitfield barriers);

  /* WebGLRenderingContextBase overrides */
  void InitializeNewContext() override;
  ScriptValue getParameter(ScriptState*, GLenum pname) override;

  /* WebGL2RenderingContextBase overrides */
  ScriptValue getIndexedParameter(ScriptState*,
                                  GLenum target,
                                  GLuint index) override;

  void Trace(blink::Visitor*) override;

 protected:
  WebGL2ComputeRenderingContextBase(
      CanvasRenderingContextHost*,
      std::unique_ptr<WebGraphicsContext3DProvider>,
      bool using_gpu_compositing,
      const CanvasContextCreationAttributesCore& requested_attributes);

  virtual bool ValidateProgramInterface(const char* function_name,
                                        GLenum program_interface);
  virtual bool ValidateProgramResourceIndex(const char* function_name,
                                            WebGLProgram*,
                                            GLenum program_interface,
                                            GLuint index);
  virtual bool ValidateAndExtendProgramResourceProperties(
      const char* function_name,
      GLenum program_interface,
      const Vector<GLenum>& props,
      Vector<GLenum>& extended_props);

  ScriptValue WrapLocation(ScriptState*,
                           GLint location,
                           WebGLProgram* program,
                           GLenum program_interface);

  /* WebGLRenderingContextBase overrides */
  bool ValidateShaderType(const char* function_name,
                          GLenum shader_type) override;
  bool ValidateBufferTarget(const char* function_name, GLenum target) override;
  WebGLBuffer* ValidateBufferDataTarget(const char* function_name,
                                        GLenum target) override;
  bool ValidateAndUpdateBufferBindTarget(const char* function_name,
                                         GLenum target,
                                         WebGLBuffer*) override;
  void RemoveBoundBuffer(WebGLBuffer*) override;

  /* WebGL2RenderingContextBase overrides */
  bool ValidateBufferTargetCompatibility(const char* function_name,
                                         GLenum target,
                                         WebGLBuffer*) override;
  bool ValidateBufferBaseTarget(const char* function_name,
                                GLenum target) override;
  bool ValidateAndUpdateBufferBindBaseTarget(const char* function_name,
                                             GLenum target,
                                             GLuint index,
                                             WebGLBuffer*) override;

  Member<WebGLBuffer> bound_dispatch_indirect_buffer_;
  Member<WebGLBuffer> bound_draw_indirect_buffer_;
  Member<WebGLBuffer> bound_atomic_counter_buffer_;
  Member<WebGLBuffer> bound_shader_storage_buffer_;

  HeapVector<Member<WebGLBuffer>> bound_indexed_atomic_counter_buffers_;
  HeapVector<Member<WebGLBuffer>> bound_indexed_shader_storage_buffers_;
};

DEFINE_TYPE_CASTS(WebGL2ComputeRenderingContextBase,
                  CanvasRenderingContext,
                  context,
                  context->Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(context) ==
                          Platform::kWebGL2ComputeContextType,
                  context.Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(&context) ==
                          Platform::kWebGL2ComputeContextType);

}  // namespace blink

#endif
