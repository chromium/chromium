// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"

using gpu::gles2::GLES2Interface;
using gpu::ContextSupport;

namespace {

class ScopedCallingGLFromSkia {
 public:
  ScopedCallingGLFromSkia(ContextSupport* context_support)
      : context_support_(context_support) {
    context_support_->WillCallGLFromSkia();
  }
  ~ScopedCallingGLFromSkia() { context_support_->DidCallGLFromSkia(); }

 private:
  raw_ptr<ContextSupport> context_support_;
};

template <typename R, typename... Args>
GrGLFunction<R GR_GL_FUNCTION_TYPE(Args...)> gles_bind(
    R (GLES2Interface::*func)(Args...),
    GLES2Interface* gles2_interface,
    ContextSupport* context_support) {
  if (context_support->HasGrContextSupport()) {
    return [func, context_support, gles2_interface](Args... args) {
      ScopedCallingGLFromSkia guard(context_support);
      return (gles2_interface->*func)(args...);
    };
  }

  // This fallback binding should only be used by unit tests which do not care
  // about GrContext::resetContext() getting called automatically.
  return [func, gles2_interface](Args... args) {
    return (gles2_interface->*func)(args...);
  };
}
}  // namespace

namespace skia_bindings {

sk_sp<GrGLInterface> CreateGLES2InterfaceBindings(
    GLES2Interface* impl,
    ContextSupport* context_support) {
  sk_sp<GrGLInterface> interface(new GrGLInterface);
  interface->fStandard = kGLES_GrGLStandard;

  // Allowing Skia to use GL ES 3.0 caused a perf test regression (see Issue
  // 719572). Until that can be understood we are limiting to ES 2.0 (with
  // extensions).
  auto get_string = [impl](GLenum name) {
    if (name == GL_VERSION)
      return reinterpret_cast<const GLubyte*>("OpenGL ES 2.0 Chromium");
    return impl->GetString(name);
  };
  auto get_stringi =
      gles_bind(&GLES2Interface::GetStringi, impl, context_support);
  auto get_integerv =
      gles_bind(&GLES2Interface::GetIntegerv, impl, context_support);
  interface->fExtensions.init(kGLES_GrGLStandard, get_string, get_stringi,
                              get_integerv);

  GrGLInterface::Functions* functions = &interface->fFunctions;
  functions->fActiveTexture =
      gles_bind(&GLES2Interface::ActiveTexture, impl, context_support);
  functions->fAttachShader =
      gles_bind(&GLES2Interface::AttachShader, impl, context_support);
  functions->fBindAttribLocation =
      gles_bind(&GLES2Interface::BindAttribLocation, impl, context_support);
  functions->fBindBuffer =
      gles_bind(&GLES2Interface::BindBuffer, impl, context_support);
  functions->fBindTexture =
      gles_bind(&GLES2Interface::BindTexture, impl, context_support);
  functions->fBindSampler =
      gles_bind(&GLES2Interface::BindSampler, impl, context_support);
  functions->fBindVertexArray =
      gles_bind(&GLES2Interface::BindVertexArrayOES, impl, context_support);
  functions->fBlendBarrier =
      gles_bind(&GLES2Interface::BlendBarrierKHR, impl, context_support);
  functions->fBlendColor =
      gles_bind(&GLES2Interface::BlendColor, impl, context_support);
  functions->fBlendEquation =
      gles_bind(&GLES2Interface::BlendEquation, impl, context_support);
  functions->fBlendFunc =
      gles_bind(&GLES2Interface::BlendFunc, impl, context_support);
  functions->fBufferData =
      gles_bind(&GLES2Interface::BufferData, impl, context_support);
  functions->fBufferSubData =
      gles_bind(&GLES2Interface::BufferSubData, impl, context_support);
  functions->fClear = gles_bind(&GLES2Interface::Clear, impl, context_support);
  functions->fClearColor =
      gles_bind(&GLES2Interface::ClearColor, impl, context_support);
  functions->fClearStencil =
      gles_bind(&GLES2Interface::ClearStencil, impl, context_support);
  functions->fClientWaitSync =
      gles_bind(&GLES2Interface::ClientWaitSync, impl, context_support);
  functions->fColorMask =
      gles_bind(&GLES2Interface::ColorMask, impl, context_support);
  functions->fCompileShader =
      gles_bind(&GLES2Interface::CompileShader, impl, context_support);
  functions->fCompressedTexImage2D =
      gles_bind(&GLES2Interface::CompressedTexImage2D, impl, context_support);
  functions->fCompressedTexSubImage2D = gles_bind(
      &GLES2Interface::CompressedTexSubImage2D, impl, context_support);
  functions->fCopyBufferSubData =
        gles_bind(&GLES2Interface::CopyBufferSubData, impl, context_support);
  functions->fCopyTexSubImage2D =
      gles_bind(&GLES2Interface::CopyTexSubImage2D, impl, context_support);
  functions->fCreateProgram =
      gles_bind(&GLES2Interface::CreateProgram, impl, context_support);
  functions->fCreateShader =
      gles_bind(&GLES2Interface::CreateShader, impl, context_support);
  functions->fCullFace =
      gles_bind(&GLES2Interface::CullFace, impl, context_support);
  functions->fDeleteBuffers =
      gles_bind(&GLES2Interface::DeleteBuffers, impl, context_support);
  functions->fDeleteProgram =
      gles_bind(&GLES2Interface::DeleteProgram, impl, context_support);
  functions->fDeleteSamplers =
      gles_bind(&GLES2Interface::DeleteSamplers, impl, context_support);
  functions->fDeleteShader =
      gles_bind(&GLES2Interface::DeleteShader, impl, context_support);
  functions->fDeleteSync =
      gles_bind(&GLES2Interface::DeleteSync, impl, context_support);
  functions->fDeleteTextures =
      gles_bind(&GLES2Interface::DeleteTextures, impl, context_support);
  functions->fDeleteVertexArrays =
      gles_bind(&GLES2Interface::DeleteVertexArraysOES, impl, context_support);
  functions->fDepthMask =
      gles_bind(&GLES2Interface::DepthMask, impl, context_support);
  functions->fDisable =
      gles_bind(&GLES2Interface::Disable, impl, context_support);
  functions->fDisableVertexAttribArray = gles_bind(
      &GLES2Interface::DisableVertexAttribArray, impl, context_support);
  functions->fDiscardFramebuffer =
      gles_bind(&GLES2Interface::DiscardFramebufferEXT, impl, context_support);
  functions->fDrawArrays =
      gles_bind(&GLES2Interface::DrawArrays, impl, context_support);
  functions->fDrawArraysInstanced = gles_bind(
      &GLES2Interface::DrawArraysInstancedANGLE, impl, context_support);
  functions->fDrawBuffers =
      gles_bind(&GLES2Interface::DrawBuffersEXT, impl, context_support);
  functions->fDrawElements =
      gles_bind(&GLES2Interface::DrawElements, impl, context_support);
  functions->fDrawElementsInstanced = gles_bind(
      &GLES2Interface::DrawElementsInstancedANGLE, impl, context_support);
  functions->fDrawRangeElements =
      gles_bind(&GLES2Interface::DrawRangeElements, impl, context_support);
  functions->fEnable =
      gles_bind(&GLES2Interface::Enable, impl, context_support);
  functions->fEnableVertexAttribArray = gles_bind(
      &GLES2Interface::EnableVertexAttribArray, impl, context_support);
  functions->fEndQuery =
      gles_bind(&GLES2Interface::EndQueryEXT, impl, context_support);
  functions->fFenceSync =
      gles_bind(&GLES2Interface::FenceSync, impl, context_support);
  functions->fFinish =
      gles_bind(&GLES2Interface::Finish, impl, context_support);
  functions->fFlush = gles_bind(&GLES2Interface::Flush, impl, context_support);
  functions->fFlushMappedBufferRange =
      gles_bind(&GLES2Interface::FlushMappedBufferRange, impl, context_support);
  functions->fFrontFace =
      gles_bind(&GLES2Interface::FrontFace, impl, context_support);
  functions->fGenBuffers =
      gles_bind(&GLES2Interface::GenBuffers, impl, context_support);
  functions->fGenSamplers =
      gles_bind(&GLES2Interface::GenSamplers, impl, context_support);
  functions->fGenTextures =
      gles_bind(&GLES2Interface::GenTextures, impl, context_support);
  functions->fGenVertexArrays =
      gles_bind(&GLES2Interface::GenVertexArraysOES, impl, context_support);
  functions->fGetBufferParameteriv =
      gles_bind(&GLES2Interface::GetBufferParameteriv, impl, context_support);
  functions->fGetError =
      gles_bind(&GLES2Interface::GetError, impl, context_support);
  functions->fGetFloatv =
      gles_bind(&GLES2Interface::GetFloatv, impl, context_support);
  functions->fGetIntegerv = get_integerv;
  functions->fGetInternalformativ =
      gles_bind(&GLES2Interface::GetInternalformativ, impl, context_support);
  functions->fGetProgramInfoLog =
      gles_bind(&GLES2Interface::GetProgramInfoLog, impl, context_support);
  functions->fGetProgramiv =
      gles_bind(&GLES2Interface::GetProgramiv, impl, context_support);
  functions->fGetQueryiv =
      gles_bind(&GLES2Interface::GetQueryivEXT, impl, context_support);
  functions->fGetQueryObjectuiv =
      gles_bind(&GLES2Interface::GetQueryObjectuivEXT, impl, context_support);
  functions->fGetShaderInfoLog =
      gles_bind(&GLES2Interface::GetShaderInfoLog, impl, context_support);
  functions->fGetShaderiv =
      gles_bind(&GLES2Interface::GetShaderiv, impl, context_support);
  functions->fGetShaderPrecisionFormat = gles_bind(
      &GLES2Interface::GetShaderPrecisionFormat, impl, context_support);
  functions->fGetString = get_string;
  functions->fGetStringi = get_stringi;
  functions->fGetUniformLocation =
      gles_bind(&GLES2Interface::GetUniformLocation, impl, context_support);
  functions->fInsertEventMarker =
      gles_bind(&GLES2Interface::InsertEventMarkerEXT, impl, context_support);
  functions->fInvalidateFramebuffer =
      gles_bind(&GLES2Interface::InvalidateFramebuffer, impl, context_support);
  functions->fInvalidateSubFramebuffer = gles_bind(
      &GLES2Interface::InvalidateSubFramebuffer, impl, context_support);
  functions->fIsSync =
      gles_bind(&GLES2Interface::IsSync, impl, context_support);
  functions->fIsTexture =
      gles_bind(&GLES2Interface::IsTexture, impl, context_support);
  functions->fLineWidth =
      gles_bind(&GLES2Interface::LineWidth, impl, context_support);
  functions->fLinkProgram =
      gles_bind(&GLES2Interface::LinkProgram, impl, context_support);
  functions->fMapBufferRange =
      gles_bind(&GLES2Interface::MapBufferRange, impl, context_support);
  functions->fMapBufferSubData = gles_bind(
      &GLES2Interface::MapBufferSubDataCHROMIUM, impl, context_support);
  functions->fMapTexSubImage2D = gles_bind(
      &GLES2Interface::MapTexSubImage2DCHROMIUM, impl, context_support);
  functions->fPixelStorei =
      gles_bind(&GLES2Interface::PixelStorei, impl, context_support);
  functions->fPopGroupMarker =
      gles_bind(&GLES2Interface::PopGroupMarkerEXT, impl, context_support);
  functions->fPushGroupMarker =
      gles_bind(&GLES2Interface::PushGroupMarkerEXT, impl, context_support);
  functions->fReadBuffer =
      gles_bind(&GLES2Interface::ReadBuffer, impl, context_support);
  functions->fReadPixels =
      gles_bind(&GLES2Interface::ReadPixels, impl, context_support);
  functions->fSamplerParameterf =
      gles_bind(&GLES2Interface::SamplerParameterf, impl, context_support);
  functions->fSamplerParameteri =
      gles_bind(&GLES2Interface::SamplerParameteri, impl, context_support);
  functions->fSamplerParameteriv =
      gles_bind(&GLES2Interface::SamplerParameteriv, impl, context_support);
  functions->fScissor =
      gles_bind(&GLES2Interface::Scissor, impl, context_support);
  functions->fShaderSource =
      gles_bind(&GLES2Interface::ShaderSource, impl, context_support);
  functions->fStencilFunc =
      gles_bind(&GLES2Interface::StencilFunc, impl, context_support);
  functions->fStencilFuncSeparate =
      gles_bind(&GLES2Interface::StencilFuncSeparate, impl, context_support);
  functions->fStencilMask =
      gles_bind(&GLES2Interface::StencilMask, impl, context_support);
  functions->fStencilMaskSeparate =
      gles_bind(&GLES2Interface::StencilMaskSeparate, impl, context_support);
  functions->fStencilOp =
      gles_bind(&GLES2Interface::StencilOp, impl, context_support);
  functions->fStencilOpSeparate =
      gles_bind(&GLES2Interface::StencilOpSeparate, impl, context_support);
  functions->fTexImage2D =
      gles_bind(&GLES2Interface::TexImage2D, impl, context_support);
  functions->fTexParameterf =
      gles_bind(&GLES2Interface::TexParameterf, impl, context_support);
  functions->fTexParameterfv =
      gles_bind(&GLES2Interface::TexParameterfv, impl, context_support);
  functions->fTexParameteri =
      gles_bind(&GLES2Interface::TexParameteri, impl, context_support);
  functions->fTexParameteriv =
      gles_bind(&GLES2Interface::TexParameteriv, impl, context_support);
  functions->fTexStorage2D =
      gles_bind(&GLES2Interface::TexStorage2DEXT, impl, context_support);
  functions->fTexSubImage2D =
      gles_bind(&GLES2Interface::TexSubImage2D, impl, context_support);
  functions->fUniform1f =
      gles_bind(&GLES2Interface::Uniform1f, impl, context_support);
  functions->fUniform1i =
      gles_bind(&GLES2Interface::Uniform1i, impl, context_support);
  functions->fUniform1fv =
      gles_bind(&GLES2Interface::Uniform1fv, impl, context_support);
  functions->fUniform1iv =
      gles_bind(&GLES2Interface::Uniform1iv, impl, context_support);
  functions->fUniform2f =
      gles_bind(&GLES2Interface::Uniform2f, impl, context_support);
  functions->fUniform2i =
      gles_bind(&GLES2Interface::Uniform2i, impl, context_support);
  functions->fUniform2fv =
      gles_bind(&GLES2Interface::Uniform2fv, impl, context_support);
  functions->fUniform2iv =
      gles_bind(&GLES2Interface::Uniform2iv, impl, context_support);
  functions->fUniform3f =
      gles_bind(&GLES2Interface::Uniform3f, impl, context_support);
  functions->fUniform3i =
      gles_bind(&GLES2Interface::Uniform3i, impl, context_support);
  functions->fUniform3fv =
      gles_bind(&GLES2Interface::Uniform3fv, impl, context_support);
  functions->fUniform3iv =
      gles_bind(&GLES2Interface::Uniform3iv, impl, context_support);
  functions->fUniform4f =
      gles_bind(&GLES2Interface::Uniform4f, impl, context_support);
  functions->fUniform4i =
      gles_bind(&GLES2Interface::Uniform4i, impl, context_support);
  functions->fUniform4fv =
      gles_bind(&GLES2Interface::Uniform4fv, impl, context_support);
  functions->fUniform4iv =
      gles_bind(&GLES2Interface::Uniform4iv, impl, context_support);
  functions->fUniformMatrix2fv =
      gles_bind(&GLES2Interface::UniformMatrix2fv, impl, context_support);
  functions->fUniformMatrix3fv =
      gles_bind(&GLES2Interface::UniformMatrix3fv, impl, context_support);
  functions->fUniformMatrix4fv =
      gles_bind(&GLES2Interface::UniformMatrix4fv, impl, context_support);
  functions->fUnmapBufferSubData = gles_bind(
      &GLES2Interface::UnmapBufferSubDataCHROMIUM, impl, context_support);
  functions->fUnmapTexSubImage2D = gles_bind(
      &GLES2Interface::UnmapTexSubImage2DCHROMIUM, impl, context_support);
  functions->fUseProgram =
      gles_bind(&GLES2Interface::UseProgram, impl, context_support);
  functions->fVertexAttrib1f =
      gles_bind(&GLES2Interface::VertexAttrib1f, impl, context_support);
  functions->fVertexAttrib2fv =
      gles_bind(&GLES2Interface::VertexAttrib2fv, impl, context_support);
  functions->fVertexAttrib3fv =
      gles_bind(&GLES2Interface::VertexAttrib3fv, impl, context_support);
  functions->fVertexAttrib4fv =
      gles_bind(&GLES2Interface::VertexAttrib4fv, impl, context_support);
  functions->fVertexAttribDivisor = gles_bind(
      &GLES2Interface::VertexAttribDivisorANGLE, impl, context_support);
  functions->fVertexAttribPointer =
      gles_bind(&GLES2Interface::VertexAttribPointer, impl, context_support);
  functions->fVertexAttribIPointer =
      gles_bind(&GLES2Interface::VertexAttribIPointer, impl, context_support);
  functions->fViewport =
      gles_bind(&GLES2Interface::Viewport, impl, context_support);
  functions->fWaitSync =
      gles_bind(&GLES2Interface::WaitSync, impl, context_support);
  functions->fBindFramebuffer =
      gles_bind(&GLES2Interface::BindFramebuffer, impl, context_support);
  functions->fBeginQuery =
      gles_bind(&GLES2Interface::BeginQueryEXT, impl, context_support);
  functions->fBindRenderbuffer =
      gles_bind(&GLES2Interface::BindRenderbuffer, impl, context_support);
  functions->fCheckFramebufferStatus =
      gles_bind(&GLES2Interface::CheckFramebufferStatus, impl, context_support);
  functions->fDeleteFramebuffers =
      gles_bind(&GLES2Interface::DeleteFramebuffers, impl, context_support);
  functions->fDeleteQueries =
      gles_bind(&GLES2Interface::DeleteQueriesEXT, impl, context_support);
  functions->fDeleteRenderbuffers =
      gles_bind(&GLES2Interface::DeleteRenderbuffers, impl, context_support);
  functions->fFramebufferRenderbuffer = gles_bind(
      &GLES2Interface::FramebufferRenderbuffer, impl, context_support);
  functions->fFramebufferTexture2D =
      gles_bind(&GLES2Interface::FramebufferTexture2D, impl, context_support);
  functions->fFramebufferTexture2DMultisample =
      gles_bind(&GLES2Interface::FramebufferTexture2DMultisampleEXT, impl,
                context_support);
  functions->fGenFramebuffers =
      gles_bind(&GLES2Interface::GenFramebuffers, impl, context_support);
  functions->fGenRenderbuffers =
      gles_bind(&GLES2Interface::GenRenderbuffers, impl, context_support);
  functions->fGetFramebufferAttachmentParameteriv =
      gles_bind(&GLES2Interface::GetFramebufferAttachmentParameteriv, impl,
                context_support);
  functions->fGetRenderbufferParameteriv = gles_bind(
      &GLES2Interface::GetRenderbufferParameteriv, impl, context_support);
  functions->fGenQueries =
      gles_bind(&GLES2Interface::GenQueriesEXT, impl, context_support);
  functions->fRenderbufferStorage =
      gles_bind(&GLES2Interface::RenderbufferStorage, impl, context_support);
  functions->fRenderbufferStorageMultisample =
      gles_bind(&GLES2Interface::RenderbufferStorageMultisampleCHROMIUM, impl,
                context_support);
  functions->fRenderbufferStorageMultisampleES2EXT =
      gles_bind(&GLES2Interface::RenderbufferStorageMultisampleEXT, impl,
                context_support);
  functions->fBindFragDataLocation = gles_bind(
      &GLES2Interface::BindFragDataLocationEXT, impl, context_support);
  functions->fBindFragDataLocationIndexed = gles_bind(
      &GLES2Interface::BindFragDataLocationIndexedEXT, impl, context_support);
  functions->fBindUniformLocation = gles_bind(
      &GLES2Interface::BindUniformLocationCHROMIUM, impl, context_support);
  functions->fBlitFramebuffer = gles_bind(
      &GLES2Interface::BlitFramebufferCHROMIUM, impl, context_support);
  functions->fGenerateMipmap =
      gles_bind(&GLES2Interface::GenerateMipmap, impl, context_support);
  functions->fWindowRectangles =
      gles_bind(&GLES2Interface::WindowRectanglesEXT, impl, context_support);
  // Skia should not use program binaries over the command buffer. Allowing
  // clients to submit them would be unsafe, and we already cache program
  // binaries internally anyway.
  functions->fGetProgramBinary = [](GLuint, GLsizei, GLsizei*, GLenum*, void*) {
    LOG(FATAL) << "Skia shouldn't use program binaries over the command buffer";
  };
  functions->fProgramBinary = [](GLuint, GLenum, const void*, GLsizei) {
    LOG(FATAL) << "Skia shouldn't use program binaries over the command buffer";
  };
  functions->fProgramParameteri = [](GLuint, GLenum pname, GLint) {
    // This method is only used for GL_PROGRAM_BINARY_RETRIEVABLE_HINT in ES3.
    LOG(FATAL) << "Skia shouldn't use program binaries over the command buffer";
  };
  return interface;
}

}  // namespace skia_bindings
