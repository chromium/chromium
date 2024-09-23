// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_interface_stub.cc.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_STUB_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_STUB_IMPL_AUTOGEN_H_

void GLES2InterfaceStub::ActiveTexture(GLenum /* texture */) {}
void GLES2InterfaceStub::AttachShader(GLuint /* program */,
                                      GLuint /* shader */) {}
void GLES2InterfaceStub::BindAttribLocation(GLuint /* program */,
                                            GLuint /* index */,
                                            const char* /* name */) {}
void GLES2InterfaceStub::BindBuffer(GLenum /* target */, GLuint /* buffer */) {}
void GLES2InterfaceStub::BindBufferBase(GLenum /* target */,
                                        GLuint /* index */,
                                        GLuint /* buffer */) {}
void GLES2InterfaceStub::BindBufferRange(GLenum /* target */,
                                         GLuint /* index */,
                                         GLuint /* buffer */,
                                         GLintptr /* offset */,
                                         GLsizeiptr /* size */) {}
void GLES2InterfaceStub::BindFramebuffer(GLenum /* target */,
                                         GLuint /* framebuffer */) {}
void GLES2InterfaceStub::BindRenderbuffer(GLenum /* target */,
                                          GLuint /* renderbuffer */) {}
void GLES2InterfaceStub::BindSampler(GLuint /* unit */, GLuint /* sampler */) {}
void GLES2InterfaceStub::BindTexture(GLenum /* target */,
                                     GLuint /* texture */) {}
void GLES2InterfaceStub::BindTransformFeedback(GLenum /* target */,
                                               GLuint /* transformfeedback */) {
}
void GLES2InterfaceStub::BlendColor(GLclampf /* red */,
                                    GLclampf /* green */,
                                    GLclampf /* blue */,
                                    GLclampf /* alpha */) {}
void GLES2InterfaceStub::BlendEquation(GLenum /* mode */) {}
void GLES2InterfaceStub::BlendEquationSeparate(GLenum /* modeRGB */,
                                               GLenum /* modeAlpha */) {}
void GLES2InterfaceStub::BlendFunc(GLenum /* sfactor */, GLenum /* dfactor */) {
}
void GLES2InterfaceStub::BlendFuncSeparate(GLenum /* srcRGB */,
                                           GLenum /* dstRGB */,
                                           GLenum /* srcAlpha */,
                                           GLenum /* dstAlpha */) {}
void GLES2InterfaceStub::BufferData(GLenum /* target */,
                                    GLsizeiptr /* size */,
                                    const void* /* data */,
                                    GLenum /* usage */) {}
void GLES2InterfaceStub::BufferSubData(GLenum /* target */,
                                       GLintptr /* offset */,
                                       GLsizeiptr /* size */,
                                       const void* /* data */) {}
GLenum GLES2InterfaceStub::CheckFramebufferStatus(GLenum /* target */) {
  return 0;
}
void GLES2InterfaceStub::Clear(GLbitfield /* mask */) {}
void GLES2InterfaceStub::ClearBufferfi(GLenum /* buffer */,
                                       GLint /* drawbuffers */,
                                       GLfloat /* depth */,
                                       GLint /* stencil */) {}
void GLES2InterfaceStub::ClearBufferfv(GLenum /* buffer */,
                                       GLint /* drawbuffers */,
                                       const GLfloat* /* value */) {}
void GLES2InterfaceStub::ClearBufferiv(GLenum /* buffer */,
                                       GLint /* drawbuffers */,
                                       const GLint* /* value */) {}
void GLES2InterfaceStub::ClearBufferuiv(GLenum /* buffer */,
                                        GLint /* drawbuffers */,
                                        const GLuint* /* value */) {}
void GLES2InterfaceStub::ClearColor(GLclampf /* red */,
                                    GLclampf /* green */,
                                    GLclampf /* blue */,
                                    GLclampf /* alpha */) {}
void GLES2InterfaceStub::ClearDepthf(GLclampf /* depth */) {}
void GLES2InterfaceStub::ClearStencil(GLint /* s */) {}
GLenum GLES2InterfaceStub::ClientWaitSync(GLsync /* sync */,
                                          GLbitfield /* flags */,
                                          GLuint64 /* timeout */) {
  return 0;
}
void GLES2InterfaceStub::ColorMask(GLboolean /* red */,
                                   GLboolean /* green */,
                                   GLboolean /* blue */,
                                   GLboolean /* alpha */) {}
void GLES2InterfaceStub::CompileShader(GLuint /* shader */) {}
void GLES2InterfaceStub::CompressedTexImage2D(GLenum /* target */,
                                              GLint /* level */,
                                              GLenum /* internalformat */,
                                              GLsizei /* width */,
                                              GLsizei /* height */,
                                              GLint /* border */,
                                              GLsizei /* imageSize */,
                                              const void* /* data */) {}
void GLES2InterfaceStub::CompressedTexSubImage2D(GLenum /* target */,
                                                 GLint /* level */,
                                                 GLint /* xoffset */,
                                                 GLint /* yoffset */,
                                                 GLsizei /* width */,
                                                 GLsizei /* height */,
                                                 GLenum /* format */,
                                                 GLsizei /* imageSize */,
                                                 const void* /* data */) {}
void GLES2InterfaceStub::CompressedTexImage3D(GLenum /* target */,
                                              GLint /* level */,
                                              GLenum /* internalformat */,
                                              GLsizei /* width */,
                                              GLsizei /* height */,
                                              GLsizei /* depth */,
                                              GLint /* border */,
                                              GLsizei /* imageSize */,
                                              const void* /* data */) {}
void GLES2InterfaceStub::CompressedTexSubImage3D(GLenum /* target */,
                                                 GLint /* level */,
                                                 GLint /* xoffset */,
                                                 GLint /* yoffset */,
                                                 GLint /* zoffset */,
                                                 GLsizei /* width */,
                                                 GLsizei /* height */,
                                                 GLsizei /* depth */,
                                                 GLenum /* format */,
                                                 GLsizei /* imageSize */,
                                                 const void* /* data */) {}
void GLES2InterfaceStub::CopyBufferSubData(GLenum /* readtarget */,
                                           GLenum /* writetarget */,
                                           GLintptr /* readoffset */,
                                           GLintptr /* writeoffset */,
                                           GLsizeiptr /* size */) {}
void GLES2InterfaceStub::CopyTexImage2D(GLenum /* target */,
                                        GLint /* level */,
                                        GLenum /* internalformat */,
                                        GLint /* x */,
                                        GLint /* y */,
                                        GLsizei /* width */,
                                        GLsizei /* height */,
                                        GLint /* border */) {}
void GLES2InterfaceStub::CopyTexSubImage2D(GLenum /* target */,
                                           GLint /* level */,
                                           GLint /* xoffset */,
                                           GLint /* yoffset */,
                                           GLint /* x */,
                                           GLint /* y */,
                                           GLsizei /* width */,
                                           GLsizei /* height */) {}
void GLES2InterfaceStub::CopyTexSubImage3D(GLenum /* target */,
                                           GLint /* level */,
                                           GLint /* xoffset */,
                                           GLint /* yoffset */,
                                           GLint /* zoffset */,
                                           GLint /* x */,
                                           GLint /* y */,
                                           GLsizei /* width */,
                                           GLsizei /* height */) {}
GLuint GLES2InterfaceStub::CreateProgram() {
  return 0;
}
GLuint GLES2InterfaceStub::CreateShader(GLenum /* type */) {
  return 0;
}
void GLES2InterfaceStub::CullFace(GLenum /* mode */) {}
void GLES2InterfaceStub::DeleteBuffers(GLsizei /* n */,
                                       const GLuint* /* buffers */) {}
void GLES2InterfaceStub::DeleteFramebuffers(GLsizei /* n */,
                                            const GLuint* /* framebuffers */) {}
void GLES2InterfaceStub::DeleteProgram(GLuint /* program */) {}
void GLES2InterfaceStub::DeleteRenderbuffers(
    GLsizei /* n */,
    const GLuint* /* renderbuffers */) {}
void GLES2InterfaceStub::DeleteSamplers(GLsizei /* n */,
                                        const GLuint* /* samplers */) {}
void GLES2InterfaceStub::DeleteSync(GLsync /* sync */) {}
void GLES2InterfaceStub::DeleteShader(GLuint /* shader */) {}
void GLES2InterfaceStub::DeleteTextures(GLsizei /* n */,
                                        const GLuint* /* textures */) {}
void GLES2InterfaceStub::DeleteTransformFeedbacks(GLsizei /* n */,
                                                  const GLuint* /* ids */) {}
void GLES2InterfaceStub::DepthFunc(GLenum /* func */) {}
void GLES2InterfaceStub::DepthMask(GLboolean /* flag */) {}
void GLES2InterfaceStub::DepthRangef(GLclampf /* zNear */,
                                     GLclampf /* zFar */) {}
void GLES2InterfaceStub::DetachShader(GLuint /* program */,
                                      GLuint /* shader */) {}
void GLES2InterfaceStub::Disable(GLenum /* cap */) {}
void GLES2InterfaceStub::DisableVertexAttribArray(GLuint /* index */) {}
void GLES2InterfaceStub::DrawArrays(GLenum /* mode */,
                                    GLint /* first */,
                                    GLsizei /* count */) {}
void GLES2InterfaceStub::DrawElements(GLenum /* mode */,
                                      GLsizei /* count */,
                                      GLenum /* type */,
                                      const void* /* indices */) {}
void GLES2InterfaceStub::DrawRangeElements(GLenum /* mode */,
                                           GLuint /* start */,
                                           GLuint /* end */,
                                           GLsizei /* count */,
                                           GLenum /* type */,
                                           const void* /* indices */) {}
void GLES2InterfaceStub::Enable(GLenum /* cap */) {}
void GLES2InterfaceStub::EnableVertexAttribArray(GLuint /* index */) {}
GLsync GLES2InterfaceStub::FenceSync(GLenum /* condition */,
                                     GLbitfield /* flags */) {
  return 0;
}
void GLES2InterfaceStub::Finish() {}
void GLES2InterfaceStub::Flush() {}
void GLES2InterfaceStub::FramebufferRenderbuffer(
    GLenum /* target */,
    GLenum /* attachment */,
    GLenum /* renderbuffertarget */,
    GLuint /* renderbuffer */) {}
void GLES2InterfaceStub::FramebufferTexture2D(GLenum /* target */,
                                              GLenum /* attachment */,
                                              GLenum /* textarget */,
                                              GLuint /* texture */,
                                              GLint /* level */) {}
void GLES2InterfaceStub::FramebufferTextureLayer(GLenum /* target */,
                                                 GLenum /* attachment */,
                                                 GLuint /* texture */,
                                                 GLint /* level */,
                                                 GLint /* layer */) {}
void GLES2InterfaceStub::FrontFace(GLenum /* mode */) {}
void GLES2InterfaceStub::GenBuffers(GLsizei /* n */, GLuint* /* buffers */) {}
void GLES2InterfaceStub::GenerateMipmap(GLenum /* target */) {}
void GLES2InterfaceStub::GenFramebuffers(GLsizei /* n */,
                                         GLuint* /* framebuffers */) {}
void GLES2InterfaceStub::GenRenderbuffers(GLsizei /* n */,
                                          GLuint* /* renderbuffers */) {}
void GLES2InterfaceStub::GenSamplers(GLsizei /* n */, GLuint* /* samplers */) {}
void GLES2InterfaceStub::GenTextures(GLsizei /* n */, GLuint* /* textures */) {}
void GLES2InterfaceStub::GenTransformFeedbacks(GLsizei /* n */,
                                               GLuint* /* ids */) {}
void GLES2InterfaceStub::GetActiveAttrib(GLuint /* program */,
                                         GLuint /* index */,
                                         GLsizei /* bufsize */,
                                         GLsizei* /* length */,
                                         GLint* /* size */,
                                         GLenum* /* type */,
                                         char* /* name */) {}
void GLES2InterfaceStub::GetActiveUniform(GLuint /* program */,
                                          GLuint /* index */,
                                          GLsizei /* bufsize */,
                                          GLsizei* /* length */,
                                          GLint* /* size */,
                                          GLenum* /* type */,
                                          char* /* name */) {}
void GLES2InterfaceStub::GetActiveUniformBlockiv(GLuint /* program */,
                                                 GLuint /* index */,
                                                 GLenum /* pname */,
                                                 GLint* /* params */) {}
void GLES2InterfaceStub::GetActiveUniformBlockName(GLuint /* program */,
                                                   GLuint /* index */,
                                                   GLsizei /* bufsize */,
                                                   GLsizei* /* length */,
                                                   char* /* name */) {}
void GLES2InterfaceStub::GetActiveUniformsiv(GLuint /* program */,
                                             GLsizei /* count */,
                                             const GLuint* /* indices */,
                                             GLenum /* pname */,
                                             GLint* /* params */) {}
void GLES2InterfaceStub::GetAttachedShaders(GLuint /* program */,
                                            GLsizei /* maxcount */,
                                            GLsizei* /* count */,
                                            GLuint* /* shaders */) {}
GLint GLES2InterfaceStub::GetAttribLocation(GLuint /* program */,
                                            const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetBooleanv(GLenum /* pname */,
                                     GLboolean* /* params */) {}
void GLES2InterfaceStub::GetBooleani_v(GLenum /* pname */,
                                       GLuint /* index */,
                                       GLboolean* /* data */) {}
void GLES2InterfaceStub::GetBufferParameteri64v(GLenum /* target */,
                                                GLenum /* pname */,
                                                GLint64* /* params */) {}
void GLES2InterfaceStub::GetBufferParameteriv(GLenum /* target */,
                                              GLenum /* pname */,
                                              GLint* /* params */) {}
GLenum GLES2InterfaceStub::GetError() {
  return 0;
}
void GLES2InterfaceStub::GetFloatv(GLenum /* pname */, GLfloat* /* params */) {}
GLint GLES2InterfaceStub::GetFragDataLocation(GLuint /* program */,
                                              const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetFramebufferAttachmentParameteriv(
    GLenum /* target */,
    GLenum /* attachment */,
    GLenum /* pname */,
    GLint* /* params */) {}
void GLES2InterfaceStub::GetInteger64v(GLenum /* pname */,
                                       GLint64* /* params */) {}
void GLES2InterfaceStub::GetIntegeri_v(GLenum /* pname */,
                                       GLuint /* index */,
                                       GLint* /* data */) {}
void GLES2InterfaceStub::GetInteger64i_v(GLenum /* pname */,
                                         GLuint /* index */,
                                         GLint64* /* data */) {}
void GLES2InterfaceStub::GetIntegerv(GLenum /* pname */, GLint* /* params */) {}
void GLES2InterfaceStub::GetInternalformativ(GLenum /* target */,
                                             GLenum /* format */,
                                             GLenum /* pname */,
                                             GLsizei /* bufSize */,
                                             GLint* /* params */) {}
void GLES2InterfaceStub::GetProgramiv(GLuint /* program */,
                                      GLenum /* pname */,
                                      GLint* /* params */) {}
void GLES2InterfaceStub::GetProgramInfoLog(GLuint /* program */,
                                           GLsizei /* bufsize */,
                                           GLsizei* /* length */,
                                           char* /* infolog */) {}
void GLES2InterfaceStub::GetRenderbufferParameteriv(GLenum /* target */,
                                                    GLenum /* pname */,
                                                    GLint* /* params */) {}
void GLES2InterfaceStub::GetSamplerParameterfv(GLuint /* sampler */,
                                               GLenum /* pname */,
                                               GLfloat* /* params */) {}
void GLES2InterfaceStub::GetSamplerParameteriv(GLuint /* sampler */,
                                               GLenum /* pname */,
                                               GLint* /* params */) {}
void GLES2InterfaceStub::GetShaderiv(GLuint /* shader */,
                                     GLenum /* pname */,
                                     GLint* /* params */) {}
void GLES2InterfaceStub::GetShaderInfoLog(GLuint /* shader */,
                                          GLsizei /* bufsize */,
                                          GLsizei* /* length */,
                                          char* /* infolog */) {}
void GLES2InterfaceStub::GetShaderPrecisionFormat(GLenum /* shadertype */,
                                                  GLenum /* precisiontype */,
                                                  GLint* /* range */,
                                                  GLint* /* precision */) {}
void GLES2InterfaceStub::GetShaderSource(GLuint /* shader */,
                                         GLsizei /* bufsize */,
                                         GLsizei* /* length */,
                                         char* /* source */) {}
const GLubyte* GLES2InterfaceStub::GetString(GLenum /* name */) {
  return 0;
}
const GLubyte* GLES2InterfaceStub::GetStringi(GLenum /* name */,
                                              GLuint /* index */) {
  return 0;
}
void GLES2InterfaceStub::GetSynciv(GLsync /* sync */,
                                   GLenum /* pname */,
                                   GLsizei /* bufsize */,
                                   GLsizei* /* length */,
                                   GLint* /* values */) {}
void GLES2InterfaceStub::GetTexParameterfv(GLenum /* target */,
                                           GLenum /* pname */,
                                           GLfloat* /* params */) {}
void GLES2InterfaceStub::GetTexParameteriv(GLenum /* target */,
                                           GLenum /* pname */,
                                           GLint* /* params */) {}
void GLES2InterfaceStub::GetTransformFeedbackVarying(GLuint /* program */,
                                                     GLuint /* index */,
                                                     GLsizei /* bufsize */,
                                                     GLsizei* /* length */,
                                                     GLsizei* /* size */,
                                                     GLenum* /* type */,
                                                     char* /* name */) {}
GLuint GLES2InterfaceStub::GetUniformBlockIndex(GLuint /* program */,
                                                const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetUniformfv(GLuint /* program */,
                                      GLint /* location */,
                                      GLfloat* /* params */) {}
void GLES2InterfaceStub::GetUniformiv(GLuint /* program */,
                                      GLint /* location */,
                                      GLint* /* params */) {}
void GLES2InterfaceStub::GetUniformuiv(GLuint /* program */,
                                       GLint /* location */,
                                       GLuint* /* params */) {}
void GLES2InterfaceStub::GetUniformIndices(GLuint /* program */,
                                           GLsizei /* count */,
                                           const char* const* /* names */,
                                           GLuint* /* indices */) {}
GLint GLES2InterfaceStub::GetUniformLocation(GLuint /* program */,
                                             const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetVertexAttribfv(GLuint /* index */,
                                           GLenum /* pname */,
                                           GLfloat* /* params */) {}
void GLES2InterfaceStub::GetVertexAttribiv(GLuint /* index */,
                                           GLenum /* pname */,
                                           GLint* /* params */) {}
void GLES2InterfaceStub::GetVertexAttribIiv(GLuint /* index */,
                                            GLenum /* pname */,
                                            GLint* /* params */) {}
void GLES2InterfaceStub::GetVertexAttribIuiv(GLuint /* index */,
                                             GLenum /* pname */,
                                             GLuint* /* params */) {}
void GLES2InterfaceStub::GetVertexAttribPointerv(GLuint /* index */,
                                                 GLenum /* pname */,
                                                 void** /* pointer */) {}
void GLES2InterfaceStub::Hint(GLenum /* target */, GLenum /* mode */) {}
void GLES2InterfaceStub::InvalidateFramebuffer(
    GLenum /* target */,
    GLsizei /* count */,
    const GLenum* /* attachments */) {}
void GLES2InterfaceStub::InvalidateSubFramebuffer(
    GLenum /* target */,
    GLsizei /* count */,
    const GLenum* /* attachments */,
    GLint /* x */,
    GLint /* y */,
    GLsizei /* width */,
    GLsizei /* height */) {}
GLboolean GLES2InterfaceStub::IsBuffer(GLuint /* buffer */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsEnabled(GLenum /* cap */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsFramebuffer(GLuint /* framebuffer */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsProgram(GLuint /* program */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsRenderbuffer(GLuint /* renderbuffer */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsSampler(GLuint /* sampler */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsShader(GLuint /* shader */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsSync(GLsync /* sync */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsTexture(GLuint /* texture */) {
  return 0;
}
GLboolean GLES2InterfaceStub::IsTransformFeedback(
    GLuint /* transformfeedback */) {
  return 0;
}
void GLES2InterfaceStub::LineWidth(GLfloat /* width */) {}
void GLES2InterfaceStub::LinkProgram(GLuint /* program */) {}
void GLES2InterfaceStub::PauseTransformFeedback() {}
void GLES2InterfaceStub::PixelStorei(GLenum /* pname */, GLint /* param */) {}
void GLES2InterfaceStub::PolygonOffset(GLfloat /* factor */,
                                       GLfloat /* units */) {}
void GLES2InterfaceStub::ReadBuffer(GLenum /* src */) {}
void GLES2InterfaceStub::ReadPixels(GLint /* x */,
                                    GLint /* y */,
                                    GLsizei /* width */,
                                    GLsizei /* height */,
                                    GLenum /* format */,
                                    GLenum /* type */,
                                    void* /* pixels */) {}
void GLES2InterfaceStub::ReleaseShaderCompiler() {}
void GLES2InterfaceStub::RenderbufferStorage(GLenum /* target */,
                                             GLenum /* internalformat */,
                                             GLsizei /* width */,
                                             GLsizei /* height */) {}
void GLES2InterfaceStub::ResumeTransformFeedback() {}
void GLES2InterfaceStub::SampleCoverage(GLclampf /* value */,
                                        GLboolean /* invert */) {}
void GLES2InterfaceStub::SamplerParameterf(GLuint /* sampler */,
                                           GLenum /* pname */,
                                           GLfloat /* param */) {}
void GLES2InterfaceStub::SamplerParameterfv(GLuint /* sampler */,
                                            GLenum /* pname */,
                                            const GLfloat* /* params */) {}
void GLES2InterfaceStub::SamplerParameteri(GLuint /* sampler */,
                                           GLenum /* pname */,
                                           GLint /* param */) {}
void GLES2InterfaceStub::SamplerParameteriv(GLuint /* sampler */,
                                            GLenum /* pname */,
                                            const GLint* /* params */) {}
void GLES2InterfaceStub::Scissor(GLint /* x */,
                                 GLint /* y */,
                                 GLsizei /* width */,
                                 GLsizei /* height */) {}
void GLES2InterfaceStub::ShaderBinary(GLsizei /* n */,
                                      const GLuint* /* shaders */,
                                      GLenum /* binaryformat */,
                                      const void* /* binary */,
                                      GLsizei /* length */) {}
void GLES2InterfaceStub::ShaderSource(GLuint /* shader */,
                                      GLsizei /* count */,
                                      const GLchar* const* /* str */,
                                      const GLint* /* length */) {}
void GLES2InterfaceStub::ShallowFinishCHROMIUM() {}
void GLES2InterfaceStub::OrderingBarrierCHROMIUM() {}
void GLES2InterfaceStub::MultiDrawArraysWEBGL(GLenum /* mode */,
                                              const GLint* /* firsts */,
                                              const GLsizei* /* counts */,
                                              GLsizei /* drawcount */) {}
void GLES2InterfaceStub::MultiDrawArraysInstancedWEBGL(
    GLenum /* mode */,
    const GLint* /* firsts */,
    const GLsizei* /* counts */,
    const GLsizei* /* instance_counts */,
    GLsizei /* drawcount */) {}
void GLES2InterfaceStub::MultiDrawArraysInstancedBaseInstanceWEBGL(
    GLenum /* mode */,
    const GLint* /* firsts */,
    const GLsizei* /* counts */,
    const GLsizei* /* instance_counts */,
    const GLuint* /* baseinstances */,
    GLsizei /* drawcount */) {}
void GLES2InterfaceStub::MultiDrawElementsWEBGL(GLenum /* mode */,
                                                const GLsizei* /* counts */,
                                                GLenum /* type */,
                                                const GLsizei* /* offsets */,
                                                GLsizei /* drawcount */) {}
void GLES2InterfaceStub::MultiDrawElementsInstancedWEBGL(
    GLenum /* mode */,
    const GLsizei* /* counts */,
    GLenum /* type */,
    const GLsizei* /* offsets */,
    const GLsizei* /* instance_counts */,
    GLsizei /* drawcount */) {}
void GLES2InterfaceStub::MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
    GLenum /* mode */,
    const GLsizei* /* counts */,
    GLenum /* type */,
    const GLsizei* /* offsets */,
    const GLsizei* /* instance_counts */,
    const GLint* /* basevertices */,
    const GLuint* /* baseinstances */,
    GLsizei /* drawcount */) {}
void GLES2InterfaceStub::StencilFunc(GLenum /* func */,
                                     GLint /* ref */,
                                     GLuint /* mask */) {}
void GLES2InterfaceStub::StencilFuncSeparate(GLenum /* face */,
                                             GLenum /* func */,
                                             GLint /* ref */,
                                             GLuint /* mask */) {}
void GLES2InterfaceStub::StencilMask(GLuint /* mask */) {}
void GLES2InterfaceStub::StencilMaskSeparate(GLenum /* face */,
                                             GLuint /* mask */) {}
void GLES2InterfaceStub::StencilOp(GLenum /* fail */,
                                   GLenum /* zfail */,
                                   GLenum /* zpass */) {}
void GLES2InterfaceStub::StencilOpSeparate(GLenum /* face */,
                                           GLenum /* fail */,
                                           GLenum /* zfail */,
                                           GLenum /* zpass */) {}
void GLES2InterfaceStub::TexImage2D(GLenum /* target */,
                                    GLint /* level */,
                                    GLint /* internalformat */,
                                    GLsizei /* width */,
                                    GLsizei /* height */,
                                    GLint /* border */,
                                    GLenum /* format */,
                                    GLenum /* type */,
                                    const void* /* pixels */) {}
void GLES2InterfaceStub::TexImage3D(GLenum /* target */,
                                    GLint /* level */,
                                    GLint /* internalformat */,
                                    GLsizei /* width */,
                                    GLsizei /* height */,
                                    GLsizei /* depth */,
                                    GLint /* border */,
                                    GLenum /* format */,
                                    GLenum /* type */,
                                    const void* /* pixels */) {}
void GLES2InterfaceStub::TexParameterf(GLenum /* target */,
                                       GLenum /* pname */,
                                       GLfloat /* param */) {}
void GLES2InterfaceStub::TexParameterfv(GLenum /* target */,
                                        GLenum /* pname */,
                                        const GLfloat* /* params */) {}
void GLES2InterfaceStub::TexParameteri(GLenum /* target */,
                                       GLenum /* pname */,
                                       GLint /* param */) {}
void GLES2InterfaceStub::TexParameteriv(GLenum /* target */,
                                        GLenum /* pname */,
                                        const GLint* /* params */) {}
void GLES2InterfaceStub::TexStorage3D(GLenum /* target */,
                                      GLsizei /* levels */,
                                      GLenum /* internalFormat */,
                                      GLsizei /* width */,
                                      GLsizei /* height */,
                                      GLsizei /* depth */) {}
void GLES2InterfaceStub::TexSubImage2D(GLenum /* target */,
                                       GLint /* level */,
                                       GLint /* xoffset */,
                                       GLint /* yoffset */,
                                       GLsizei /* width */,
                                       GLsizei /* height */,
                                       GLenum /* format */,
                                       GLenum /* type */,
                                       const void* /* pixels */) {}
void GLES2InterfaceStub::TexSubImage3D(GLenum /* target */,
                                       GLint /* level */,
                                       GLint /* xoffset */,
                                       GLint /* yoffset */,
                                       GLint /* zoffset */,
                                       GLsizei /* width */,
                                       GLsizei /* height */,
                                       GLsizei /* depth */,
                                       GLenum /* format */,
                                       GLenum /* type */,
                                       const void* /* pixels */) {}
void GLES2InterfaceStub::TransformFeedbackVaryings(
    GLuint /* program */,
    GLsizei /* count */,
    const char* const* /* varyings */,
    GLenum /* buffermode */) {}
void GLES2InterfaceStub::Uniform1f(GLint /* location */, GLfloat /* x */) {}
void GLES2InterfaceStub::Uniform1fv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLfloat* /* v */) {}
void GLES2InterfaceStub::Uniform1i(GLint /* location */, GLint /* x */) {}
void GLES2InterfaceStub::Uniform1iv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLint* /* v */) {}
void GLES2InterfaceStub::Uniform1ui(GLint /* location */, GLuint /* x */) {}
void GLES2InterfaceStub::Uniform1uiv(GLint /* location */,
                                     GLsizei /* count */,
                                     const GLuint* /* v */) {}
void GLES2InterfaceStub::Uniform2f(GLint /* location */,
                                   GLfloat /* x */,
                                   GLfloat /* y */) {}
void GLES2InterfaceStub::Uniform2fv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLfloat* /* v */) {}
void GLES2InterfaceStub::Uniform2i(GLint /* location */,
                                   GLint /* x */,
                                   GLint /* y */) {}
void GLES2InterfaceStub::Uniform2iv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLint* /* v */) {}
void GLES2InterfaceStub::Uniform2ui(GLint /* location */,
                                    GLuint /* x */,
                                    GLuint /* y */) {}
void GLES2InterfaceStub::Uniform2uiv(GLint /* location */,
                                     GLsizei /* count */,
                                     const GLuint* /* v */) {}
void GLES2InterfaceStub::Uniform3f(GLint /* location */,
                                   GLfloat /* x */,
                                   GLfloat /* y */,
                                   GLfloat /* z */) {}
void GLES2InterfaceStub::Uniform3fv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLfloat* /* v */) {}
void GLES2InterfaceStub::Uniform3i(GLint /* location */,
                                   GLint /* x */,
                                   GLint /* y */,
                                   GLint /* z */) {}
void GLES2InterfaceStub::Uniform3iv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLint* /* v */) {}
void GLES2InterfaceStub::Uniform3ui(GLint /* location */,
                                    GLuint /* x */,
                                    GLuint /* y */,
                                    GLuint /* z */) {}
void GLES2InterfaceStub::Uniform3uiv(GLint /* location */,
                                     GLsizei /* count */,
                                     const GLuint* /* v */) {}
void GLES2InterfaceStub::Uniform4f(GLint /* location */,
                                   GLfloat /* x */,
                                   GLfloat /* y */,
                                   GLfloat /* z */,
                                   GLfloat /* w */) {}
void GLES2InterfaceStub::Uniform4fv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLfloat* /* v */) {}
void GLES2InterfaceStub::Uniform4i(GLint /* location */,
                                   GLint /* x */,
                                   GLint /* y */,
                                   GLint /* z */,
                                   GLint /* w */) {}
void GLES2InterfaceStub::Uniform4iv(GLint /* location */,
                                    GLsizei /* count */,
                                    const GLint* /* v */) {}
void GLES2InterfaceStub::Uniform4ui(GLint /* location */,
                                    GLuint /* x */,
                                    GLuint /* y */,
                                    GLuint /* z */,
                                    GLuint /* w */) {}
void GLES2InterfaceStub::Uniform4uiv(GLint /* location */,
                                     GLsizei /* count */,
                                     const GLuint* /* v */) {}
void GLES2InterfaceStub::UniformBlockBinding(GLuint /* program */,
                                             GLuint /* index */,
                                             GLuint /* binding */) {}
void GLES2InterfaceStub::UniformMatrix2fv(GLint /* location */,
                                          GLsizei /* count */,
                                          GLboolean /* transpose */,
                                          const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix2x3fv(GLint /* location */,
                                            GLsizei /* count */,
                                            GLboolean /* transpose */,
                                            const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix2x4fv(GLint /* location */,
                                            GLsizei /* count */,
                                            GLboolean /* transpose */,
                                            const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix3fv(GLint /* location */,
                                          GLsizei /* count */,
                                          GLboolean /* transpose */,
                                          const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix3x2fv(GLint /* location */,
                                            GLsizei /* count */,
                                            GLboolean /* transpose */,
                                            const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix3x4fv(GLint /* location */,
                                            GLsizei /* count */,
                                            GLboolean /* transpose */,
                                            const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix4fv(GLint /* location */,
                                          GLsizei /* count */,
                                          GLboolean /* transpose */,
                                          const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix4x2fv(GLint /* location */,
                                            GLsizei /* count */,
                                            GLboolean /* transpose */,
                                            const GLfloat* /* value */) {}
void GLES2InterfaceStub::UniformMatrix4x3fv(GLint /* location */,
                                            GLsizei /* count */,
                                            GLboolean /* transpose */,
                                            const GLfloat* /* value */) {}
void GLES2InterfaceStub::UseProgram(GLuint /* program */) {}
void GLES2InterfaceStub::ValidateProgram(GLuint /* program */) {}
void GLES2InterfaceStub::VertexAttrib1f(GLuint /* indx */, GLfloat /* x */) {}
void GLES2InterfaceStub::VertexAttrib1fv(GLuint /* indx */,
                                         const GLfloat* /* values */) {}
void GLES2InterfaceStub::VertexAttrib2f(GLuint /* indx */,
                                        GLfloat /* x */,
                                        GLfloat /* y */) {}
void GLES2InterfaceStub::VertexAttrib2fv(GLuint /* indx */,
                                         const GLfloat* /* values */) {}
void GLES2InterfaceStub::VertexAttrib3f(GLuint /* indx */,
                                        GLfloat /* x */,
                                        GLfloat /* y */,
                                        GLfloat /* z */) {}
void GLES2InterfaceStub::VertexAttrib3fv(GLuint /* indx */,
                                         const GLfloat* /* values */) {}
void GLES2InterfaceStub::VertexAttrib4f(GLuint /* indx */,
                                        GLfloat /* x */,
                                        GLfloat /* y */,
                                        GLfloat /* z */,
                                        GLfloat /* w */) {}
void GLES2InterfaceStub::VertexAttrib4fv(GLuint /* indx */,
                                         const GLfloat* /* values */) {}
void GLES2InterfaceStub::VertexAttribI4i(GLuint /* indx */,
                                         GLint /* x */,
                                         GLint /* y */,
                                         GLint /* z */,
                                         GLint /* w */) {}
void GLES2InterfaceStub::VertexAttribI4iv(GLuint /* indx */,
                                          const GLint* /* values */) {}
void GLES2InterfaceStub::VertexAttribI4ui(GLuint /* indx */,
                                          GLuint /* x */,
                                          GLuint /* y */,
                                          GLuint /* z */,
                                          GLuint /* w */) {}
void GLES2InterfaceStub::VertexAttribI4uiv(GLuint /* indx */,
                                           const GLuint* /* values */) {}
void GLES2InterfaceStub::VertexAttribIPointer(GLuint /* indx */,
                                              GLint /* size */,
                                              GLenum /* type */,
                                              GLsizei /* stride */,
                                              const void* /* ptr */) {}
void GLES2InterfaceStub::VertexAttribPointer(GLuint /* indx */,
                                             GLint /* size */,
                                             GLenum /* type */,
                                             GLboolean /* normalized */,
                                             GLsizei /* stride */,
                                             const void* /* ptr */) {}
void GLES2InterfaceStub::Viewport(GLint /* x */,
                                  GLint /* y */,
                                  GLsizei /* width */,
                                  GLsizei /* height */) {}
void GLES2InterfaceStub::WaitSync(GLsync /* sync */,
                                  GLbitfield /* flags */,
                                  GLuint64 /* timeout */) {}
void GLES2InterfaceStub::BlitFramebufferCHROMIUM(GLint /* srcX0 */,
                                                 GLint /* srcY0 */,
                                                 GLint /* srcX1 */,
                                                 GLint /* srcY1 */,
                                                 GLint /* dstX0 */,
                                                 GLint /* dstY0 */,
                                                 GLint /* dstX1 */,
                                                 GLint /* dstY1 */,
                                                 GLbitfield /* mask */,
                                                 GLenum /* filter */) {}
void GLES2InterfaceStub::RenderbufferStorageMultisampleCHROMIUM(
    GLenum /* target */,
    GLsizei /* samples */,
    GLenum /* internalformat */,
    GLsizei /* width */,
    GLsizei /* height */) {}
void GLES2InterfaceStub::RenderbufferStorageMultisampleAdvancedAMD(
    GLenum /* target */,
    GLsizei /* samples */,
    GLsizei /* storageSamples */,
    GLenum /* internalformat */,
    GLsizei /* width */,
    GLsizei /* height */) {}
void GLES2InterfaceStub::RenderbufferStorageMultisampleEXT(
    GLenum /* target */,
    GLsizei /* samples */,
    GLenum /* internalformat */,
    GLsizei /* width */,
    GLsizei /* height */) {}
void GLES2InterfaceStub::FramebufferTexture2DMultisampleEXT(
    GLenum /* target */,
    GLenum /* attachment */,
    GLenum /* textarget */,
    GLuint /* texture */,
    GLint /* level */,
    GLsizei /* samples */) {}
void GLES2InterfaceStub::TexStorage2DEXT(GLenum /* target */,
                                         GLsizei /* levels */,
                                         GLenum /* internalFormat */,
                                         GLsizei /* width */,
                                         GLsizei /* height */) {}
void GLES2InterfaceStub::GenQueriesEXT(GLsizei /* n */, GLuint* /* queries */) {
}
void GLES2InterfaceStub::DeleteQueriesEXT(GLsizei /* n */,
                                          const GLuint* /* queries */) {}
void GLES2InterfaceStub::QueryCounterEXT(GLuint /* id */, GLenum /* target */) {
}
GLboolean GLES2InterfaceStub::IsQueryEXT(GLuint /* id */) {
  return 0;
}
void GLES2InterfaceStub::BeginQueryEXT(GLenum /* target */, GLuint /* id */) {}
void GLES2InterfaceStub::BeginTransformFeedback(GLenum /* primitivemode */) {}
void GLES2InterfaceStub::EndQueryEXT(GLenum /* target */) {}
void GLES2InterfaceStub::EndTransformFeedback() {}
void GLES2InterfaceStub::GetQueryivEXT(GLenum /* target */,
                                       GLenum /* pname */,
                                       GLint* /* params */) {}
void GLES2InterfaceStub::GetQueryObjectivEXT(GLuint /* id */,
                                             GLenum /* pname */,
                                             GLint* /* params */) {}
void GLES2InterfaceStub::GetQueryObjectuivEXT(GLuint /* id */,
                                              GLenum /* pname */,
                                              GLuint* /* params */) {}
void GLES2InterfaceStub::GetQueryObjecti64vEXT(GLuint /* id */,
                                               GLenum /* pname */,
                                               GLint64* /* params */) {}
void GLES2InterfaceStub::GetQueryObjectui64vEXT(GLuint /* id */,
                                                GLenum /* pname */,
                                                GLuint64* /* params */) {}
void GLES2InterfaceStub::SetDisjointValueSyncCHROMIUM() {}
void GLES2InterfaceStub::InsertEventMarkerEXT(GLsizei /* length */,
                                              const GLchar* /* marker */) {}
void GLES2InterfaceStub::PushGroupMarkerEXT(GLsizei /* length */,
                                            const GLchar* /* marker */) {}
void GLES2InterfaceStub::PopGroupMarkerEXT() {}
void GLES2InterfaceStub::GenVertexArraysOES(GLsizei /* n */,
                                            GLuint* /* arrays */) {}
void GLES2InterfaceStub::DeleteVertexArraysOES(GLsizei /* n */,
                                               const GLuint* /* arrays */) {}
GLboolean GLES2InterfaceStub::IsVertexArrayOES(GLuint /* array */) {
  return 0;
}
void GLES2InterfaceStub::BindVertexArrayOES(GLuint /* array */) {}
void GLES2InterfaceStub::FramebufferParameteri(GLenum /* target */,
                                               GLenum /* pname */,
                                               GLint /* param */) {}
void GLES2InterfaceStub::BindImageTexture(GLuint /* unit */,
                                          GLuint /* texture */,
                                          GLint /* level */,
                                          GLboolean /* layered */,
                                          GLint /* layer */,
                                          GLenum /* access */,
                                          GLenum /* format */) {}
void GLES2InterfaceStub::DispatchCompute(GLuint /* num_groups_x */,
                                         GLuint /* num_groups_y */,
                                         GLuint /* num_groups_z */) {}
void GLES2InterfaceStub::DispatchComputeIndirect(GLintptr /* offset */) {}
void GLES2InterfaceStub::DrawArraysIndirect(GLenum /* mode */,
                                            const void* /* offset */) {}
void GLES2InterfaceStub::DrawElementsIndirect(GLenum /* mode */,
                                              GLenum /* type */,
                                              const void* /* offset */) {}
void GLES2InterfaceStub::GetProgramInterfaceiv(GLuint /* program */,
                                               GLenum /* program_interface */,
                                               GLenum /* pname */,
                                               GLint* /* params */) {}
GLuint GLES2InterfaceStub::GetProgramResourceIndex(
    GLuint /* program */,
    GLenum /* program_interface */,
    const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::GetProgramResourceName(GLuint /* program */,
                                                GLenum /* program_interface */,
                                                GLuint /* index */,
                                                GLsizei /* bufsize */,
                                                GLsizei* /* length */,
                                                char* /* name */) {}
void GLES2InterfaceStub::GetProgramResourceiv(GLuint /* program */,
                                              GLenum /* program_interface */,
                                              GLuint /* index */,
                                              GLsizei /* prop_count */,
                                              const GLenum* /* props */,
                                              GLsizei /* bufsize */,
                                              GLsizei* /* length */,
                                              GLint* /* params */) {}
GLint GLES2InterfaceStub::GetProgramResourceLocation(
    GLuint /* program */,
    GLenum /* program_interface */,
    const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::MemoryBarrierEXT(GLbitfield /* barriers */) {}
void GLES2InterfaceStub::MemoryBarrierByRegion(GLbitfield /* barriers */) {}
void GLES2InterfaceStub::SwapBuffers(GLuint64 /* swap_id */,
                                     GLbitfield /* flags */) {}
GLuint GLES2InterfaceStub::GetMaxValueInBufferCHROMIUM(GLuint /* buffer_id */,
                                                       GLsizei /* count */,
                                                       GLenum /* type */,
                                                       GLuint /* offset */) {
  return 0;
}
GLboolean GLES2InterfaceStub::EnableFeatureCHROMIUM(const char* /* feature */) {
  return 0;
}
void* GLES2InterfaceStub::MapBufferCHROMIUM(GLuint /* target */,
                                            GLenum /* access */) {
  return 0;
}
GLboolean GLES2InterfaceStub::UnmapBufferCHROMIUM(GLuint /* target */) {
  return 0;
}
void* GLES2InterfaceStub::MapBufferSubDataCHROMIUM(GLuint /* target */,
                                                   GLintptr /* offset */,
                                                   GLsizeiptr /* size */,
                                                   GLenum /* access */) {
  return 0;
}
void GLES2InterfaceStub::UnmapBufferSubDataCHROMIUM(const void* /* mem */) {}
void* GLES2InterfaceStub::MapBufferRange(GLenum /* target */,
                                         GLintptr /* offset */,
                                         GLsizeiptr /* size */,
                                         GLbitfield /* access */) {
  return 0;
}
GLboolean GLES2InterfaceStub::UnmapBuffer(GLenum /* target */) {
  return 0;
}
void GLES2InterfaceStub::FlushMappedBufferRange(GLenum /* target */,
                                                GLintptr /* offset */,
                                                GLsizeiptr /* size */) {}
void* GLES2InterfaceStub::MapTexSubImage2DCHROMIUM(GLenum /* target */,
                                                   GLint /* level */,
                                                   GLint /* xoffset */,
                                                   GLint /* yoffset */,
                                                   GLsizei /* width */,
                                                   GLsizei /* height */,
                                                   GLenum /* format */,
                                                   GLenum /* type */,
                                                   GLenum /* access */) {
  return 0;
}
void GLES2InterfaceStub::UnmapTexSubImage2DCHROMIUM(const void* /* mem */) {}
void GLES2InterfaceStub::ResizeCHROMIUM(GLuint /* width */,
                                        GLuint /* height */,
                                        GLfloat /* scale_factor */,
                                        GLcolorSpace /* color_space */,
                                        GLboolean /* alpha */) {}
const GLchar* GLES2InterfaceStub::GetRequestableExtensionsCHROMIUM() {
  return 0;
}
void GLES2InterfaceStub::RequestExtensionCHROMIUM(const char* /* extension */) {
}
void GLES2InterfaceStub::GetProgramInfoCHROMIUM(GLuint /* program */,
                                                GLsizei /* bufsize */,
                                                GLsizei* /* size */,
                                                void* /* info */) {}
void GLES2InterfaceStub::GetUniformBlocksCHROMIUM(GLuint /* program */,
                                                  GLsizei /* bufsize */,
                                                  GLsizei* /* size */,
                                                  void* /* info */) {}
void GLES2InterfaceStub::GetTransformFeedbackVaryingsCHROMIUM(
    GLuint /* program */,
    GLsizei /* bufsize */,
    GLsizei* /* size */,
    void* /* info */) {}
void GLES2InterfaceStub::GetUniformsES3CHROMIUM(GLuint /* program */,
                                                GLsizei /* bufsize */,
                                                GLsizei* /* size */,
                                                void* /* info */) {}
void GLES2InterfaceStub::DescheduleUntilFinishedCHROMIUM() {}
void GLES2InterfaceStub::GetTranslatedShaderSourceANGLE(GLuint /* shader */,
                                                        GLsizei /* bufsize */,
                                                        GLsizei* /* length */,
                                                        char* /* source */) {}
void GLES2InterfaceStub::CopyTextureCHROMIUM(
    GLuint /* source_id */,
    GLint /* source_level */,
    GLenum /* dest_target */,
    GLuint /* dest_id */,
    GLint /* dest_level */,
    GLint /* internalformat */,
    GLenum /* dest_type */,
    GLboolean /* unpack_flip_y */,
    GLboolean /* unpack_premultiply_alpha */,
    GLboolean /* unpack_unmultiply_alpha */) {}
void GLES2InterfaceStub::CopySubTextureCHROMIUM(
    GLuint /* source_id */,
    GLint /* source_level */,
    GLenum /* dest_target */,
    GLuint /* dest_id */,
    GLint /* dest_level */,
    GLint /* xoffset */,
    GLint /* yoffset */,
    GLint /* x */,
    GLint /* y */,
    GLsizei /* width */,
    GLsizei /* height */,
    GLboolean /* unpack_flip_y */,
    GLboolean /* unpack_premultiply_alpha */,
    GLboolean /* unpack_unmultiply_alpha */) {}
void GLES2InterfaceStub::DrawArraysInstancedANGLE(GLenum /* mode */,
                                                  GLint /* first */,
                                                  GLsizei /* count */,
                                                  GLsizei /* primcount */) {}
void GLES2InterfaceStub::DrawArraysInstancedBaseInstanceANGLE(
    GLenum /* mode */,
    GLint /* first */,
    GLsizei /* count */,
    GLsizei /* primcount */,
    GLuint /* baseinstance */) {}
void GLES2InterfaceStub::DrawElementsInstancedANGLE(GLenum /* mode */,
                                                    GLsizei /* count */,
                                                    GLenum /* type */,
                                                    const void* /* indices */,
                                                    GLsizei /* primcount */) {}
void GLES2InterfaceStub::DrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum /* mode */,
    GLsizei /* count */,
    GLenum /* type */,
    const void* /* indices */,
    GLsizei /* primcount */,
    GLint /* basevertex */,
    GLuint /* baseinstance */) {}
void GLES2InterfaceStub::VertexAttribDivisorANGLE(GLuint /* index */,
                                                  GLuint /* divisor */) {}
void GLES2InterfaceStub::BindUniformLocationCHROMIUM(GLuint /* program */,
                                                     GLint /* location */,
                                                     const char* /* name */) {}
void GLES2InterfaceStub::TraceBeginCHROMIUM(const char* /* category_name */,
                                            const char* /* trace_name */) {}
void GLES2InterfaceStub::TraceEndCHROMIUM() {}
void GLES2InterfaceStub::DiscardFramebufferEXT(
    GLenum /* target */,
    GLsizei /* count */,
    const GLenum* /* attachments */) {}
void GLES2InterfaceStub::LoseContextCHROMIUM(GLenum /* current */,
                                             GLenum /* other */) {}
void GLES2InterfaceStub::DrawBuffersEXT(GLsizei /* count */,
                                        const GLenum* /* bufs */) {}
void GLES2InterfaceStub::FlushDriverCachesCHROMIUM() {}
GLuint GLES2InterfaceStub::GetLastFlushIdCHROMIUM() {
  return 0;
}
void GLES2InterfaceStub::SetActiveURLCHROMIUM(const char* /* url */) {}
void GLES2InterfaceStub::ContextVisibilityHintCHROMIUM(
    GLboolean /* visibility */) {}
GLenum GLES2InterfaceStub::GetGraphicsResetStatusKHR() {
  return 0;
}
void GLES2InterfaceStub::BlendBarrierKHR() {}
void GLES2InterfaceStub::BindFragDataLocationIndexedEXT(
    GLuint /* program */,
    GLuint /* colorNumber */,
    GLuint /* index */,
    const char* /* name */) {}
void GLES2InterfaceStub::BindFragDataLocationEXT(GLuint /* program */,
                                                 GLuint /* colorNumber */,
                                                 const char* /* name */) {}
GLint GLES2InterfaceStub::GetFragDataIndexEXT(GLuint /* program */,
                                              const char* /* name */) {
  return 0;
}
void GLES2InterfaceStub::InitializeDiscardableTextureCHROMIUM(
    GLuint /* texture_id */) {}
void GLES2InterfaceStub::UnlockDiscardableTextureCHROMIUM(
    GLuint /* texture_id */) {}
bool GLES2InterfaceStub::LockDiscardableTextureCHROMIUM(
    GLuint /* texture_id */) {
  return 0;
}
void GLES2InterfaceStub::WindowRectanglesEXT(GLenum /* mode */,
                                             GLsizei /* count */,
                                             const GLint* /* box */) {}
GLuint GLES2InterfaceStub::CreateGpuFenceCHROMIUM() {
  return 0;
}
GLuint GLES2InterfaceStub::CreateClientGpuFenceCHROMIUM(
    ClientGpuFence /* source */) {
  return 0;
}
void GLES2InterfaceStub::WaitGpuFenceCHROMIUM(GLuint /* gpu_fence_id */) {}
void GLES2InterfaceStub::DestroyGpuFenceCHROMIUM(GLuint /* gpu_fence_id */) {}
void GLES2InterfaceStub::InvalidateReadbackBufferShadowDataCHROMIUM(
    GLuint /* buffer_id */) {}
void GLES2InterfaceStub::FramebufferTextureMultiviewOVR(
    GLenum /* target */,
    GLenum /* attachment */,
    GLuint /* texture */,
    GLint /* level */,
    GLint /* baseViewIndex */,
    GLsizei /* numViews */) {}
void GLES2InterfaceStub::MaxShaderCompilerThreadsKHR(GLuint /* count */) {}
GLuint GLES2InterfaceStub::CreateAndTexStorage2DSharedImageCHROMIUM(
    const GLbyte* /* mailbox */) {
  return 0;
}
void GLES2InterfaceStub::BeginSharedImageAccessDirectCHROMIUM(
    GLuint /* texture */,
    GLenum /* mode */) {}
void GLES2InterfaceStub::EndSharedImageAccessDirectCHROMIUM(
    GLuint /* texture */) {}
void GLES2InterfaceStub::CopySharedImageINTERNAL(
    GLint /* xoffset */,
    GLint /* yoffset */,
    GLint /* x */,
    GLint /* y */,
    GLsizei /* width */,
    GLsizei /* height */,
    GLboolean /* unpack_flip_y */,
    const GLbyte* /* mailboxes */) {}
void GLES2InterfaceStub::CopySharedImageToTextureINTERNAL(
    GLuint /* texture */,
    GLenum /* target */,
    GLuint /* internal_format */,
    GLenum /* type */,
    GLint /* src_x */,
    GLint /* src_y */,
    GLsizei /* width */,
    GLsizei /* height */,
    GLboolean /* flip_y */,
    const GLbyte* /* src_mailbox */) {}
GLboolean GLES2InterfaceStub::ReadbackARGBImagePixelsINTERNAL(
    const GLbyte* /* mailbox */,
    const void* /* dst_color_space */,
    GLuint /* dst_color_space_size */,
    GLuint /* dst_size */,
    GLuint /* dst_width */,
    GLuint /* dst_height */,
    GLuint /* dst_color_type */,
    GLuint /* dst_alpha_type */,
    GLuint /* dst_row_bytes */,
    GLint /* src_x */,
    GLint /* src_y */,
    GLint /* plane_index */,
    void* /* pixels */) {
  return 0;
}
void GLES2InterfaceStub::WritePixelsYUVINTERNAL(
    const GLbyte* /* mailbox */,
    GLuint /* src_size_plane1 */,
    GLuint /* src_size_plane2 */,
    GLuint /* src_size_plane3 */,
    GLuint /* src_size_plane4 */,
    GLuint /* src_width */,
    GLuint /* src_height */,
    GLuint /* src_plane_config */,
    GLuint /* src_subsampling */,
    GLuint /* src_datatype */,
    GLuint /* src_row_bytes_plane1 */,
    GLuint /* src_row_bytes_plane2 */,
    GLuint /* src_row_bytes_plane3 */,
    GLuint /* src_row_bytes_plane4 */,
    const void* /* src_pixels_plane1 */,
    const void* /* src_pixels_plane2 */,
    const void* /* src_pixels_plane3 */,
    const void* /* src_pixels_plane4 */) {}
void GLES2InterfaceStub::EnableiOES(GLenum /* target */, GLuint /* index */) {}
void GLES2InterfaceStub::DisableiOES(GLenum /* target */, GLuint /* index */) {}
void GLES2InterfaceStub::BlendEquationiOES(GLuint /* buf */,
                                           GLenum /* mode */) {}
void GLES2InterfaceStub::BlendEquationSeparateiOES(GLuint /* buf */,
                                                   GLenum /* modeRGB */,
                                                   GLenum /* modeAlpha */) {}
void GLES2InterfaceStub::BlendFunciOES(GLuint /* buf */,
                                       GLenum /* src */,
                                       GLenum /* dst */) {}
void GLES2InterfaceStub::BlendFuncSeparateiOES(GLuint /* buf */,
                                               GLenum /* srcRGB */,
                                               GLenum /* dstRGB */,
                                               GLenum /* srcAlpha */,
                                               GLenum /* dstAlpha */) {}
void GLES2InterfaceStub::ColorMaskiOES(GLuint /* buf */,
                                       GLboolean /* r */,
                                       GLboolean /* g */,
                                       GLboolean /* b */,
                                       GLboolean /* a */) {}
GLboolean GLES2InterfaceStub::IsEnablediOES(GLenum /* target */,
                                            GLuint /* index */) {
  return 0;
}
void GLES2InterfaceStub::ProvokingVertexANGLE(GLenum /* provokeMode */) {}
void GLES2InterfaceStub::FramebufferMemorylessPixelLocalStorageANGLE(
    GLint /* plane */,
    GLenum /* internalformat */) {}
void GLES2InterfaceStub::FramebufferTexturePixelLocalStorageANGLE(
    GLint /* plane */,
    GLuint /* backingtexture */,
    GLint /* level */,
    GLint /* layer */) {}
void GLES2InterfaceStub::FramebufferPixelLocalClearValuefvANGLE(
    GLint /* plane */,
    const GLfloat* /* value */) {}
void GLES2InterfaceStub::FramebufferPixelLocalClearValueivANGLE(
    GLint /* plane */,
    const GLint* /* value */) {}
void GLES2InterfaceStub::FramebufferPixelLocalClearValueuivANGLE(
    GLint /* plane */,
    const GLuint* /* value */) {}
void GLES2InterfaceStub::BeginPixelLocalStorageANGLE(
    GLsizei /* count */,
    const GLenum* /* loadops */) {}
void GLES2InterfaceStub::EndPixelLocalStorageANGLE(
    GLsizei /* count */,
    const GLenum* /* storeops */) {}
void GLES2InterfaceStub::PixelLocalStorageBarrierANGLE() {}
void GLES2InterfaceStub::FramebufferPixelLocalStorageInterruptANGLE() {}
void GLES2InterfaceStub::FramebufferPixelLocalStorageRestoreANGLE() {}
void GLES2InterfaceStub::GetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint /* plane */,
    GLenum /* pname */,
    GLfloat* /* params */) {}
void GLES2InterfaceStub::GetFramebufferPixelLocalStorageParameterivANGLE(
    GLint /* plane */,
    GLenum /* pname */,
    GLint* /* params */) {}
void GLES2InterfaceStub::ClipControlEXT(GLenum /* origin */,
                                        GLenum /* depth */) {}
void GLES2InterfaceStub::PolygonModeANGLE(GLenum /* face */,
                                          GLenum /* mode */) {}
void GLES2InterfaceStub::PolygonOffsetClampEXT(GLfloat /* factor */,
                                               GLfloat /* units */,
                                               GLfloat /* clamp */) {}
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_INTERFACE_STUB_IMPL_AUTOGEN_H_
