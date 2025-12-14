// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_RENDERING_CONTEXT_WEBGPU_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_RENDERING_CONTEXT_WEBGPU_BASE_H_

#include <array>
#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_predefined_color_space.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webgl_context_attributes.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_object_support.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#define BINDINGS_GL_PROTOTYPES 0
#include "ui/gl/gl_bindings.h"

namespace blink {

class ExceptionState;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class ImageData;
class ScriptState;
class V8PredefinedColorSpace;
class V8UnionHTMLCanvasElementOrOffscreenCanvas;
class VideoFrame;
class WebGLActiveInfo;
class WebGLObject;
class WebGLBuffer;
class WebGLFramebuffer;
class WebGLProgram;
class WebGLQuery;
class WebGLRenderbuffer;
class WebGLSampler;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLSync;
class WebGLTexture;
class WebGLTransformFeedback;
class WebGLUniformLocation;
class WebGLVertexArrayObject;

class MODULES_EXPORT WebGLRenderingContextWebGPUBase
    : public WebGLContextObjectSupport,
      public CanvasRenderingContext,
      public WebGPUSwapBufferProvider::Client {
 public:
  WebGLRenderingContextWebGPUBase(
      CanvasRenderingContextHost* host,
      const CanvasContextCreationAttributesCore& requested_attributes,
      CanvasRenderingAPI api);
  ~WebGLRenderingContextWebGPUBase() override;

  WebGLRenderingContextWebGPUBase(const WebGLRenderingContextWebGPUBase&) =
      delete;
  WebGLRenderingContextWebGPUBase& operator=(
      const WebGLRenderingContextWebGPUBase&) = delete;

  HTMLCanvasElement* canvas() const;

  // Extra Web-exposed initAsync while until Dawn operations can be made
  // blocking in the renderer process.
  ScriptPromise<IDLUndefined> initAsync(ScriptState* script_state);

  // **************************************************************************
  // Start of WebGLRenderingContextBase's IDL methods
  // **************************************************************************
  V8UnionHTMLCanvasElementOrOffscreenCanvas* getHTMLOrOffscreenCanvas() const;

  int drawingBufferWidth() const;
  int drawingBufferHeight() const;
  GLenum drawingBufferFormat() const;
  V8PredefinedColorSpace drawingBufferColorSpace(ScriptState*) const;
  void setDrawingBufferColorSpace(ScriptState*,
                                  const V8PredefinedColorSpace& color_space,
                                  ExceptionState&);
  V8PredefinedColorSpace unpackColorSpace(ScriptState*) const;
  void setUnpackColorSpace(ScriptState*,
                           const V8PredefinedColorSpace& color_space,
                           ExceptionState&);

  void activeTexture(GLenum texture);
  void attachShader(WebGLProgram*, WebGLShader*);

  void bindAttribLocation(WebGLProgram*, GLuint index, const String& name);
  void bindBuffer(GLenum target, WebGLBuffer* buffer);
  void bindFramebuffer(GLenum target, WebGLFramebuffer*);
  void bindRenderbuffer(GLenum target, WebGLRenderbuffer*);
  void bindTexture(GLenum target, WebGLTexture*);
  void blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void blendEquation(GLenum mode);
  void blendEquationSeparate(GLenum mode_rgb, GLenum mode_alpha);
  void blendFunc(GLenum sfactor, GLenum dfactor);
  void blendFuncSeparate(GLenum src_rgb,
                         GLenum dst_rgb,
                         GLenum src_alpha,
                         GLenum dst_alpha);

  void bufferData(GLenum target, int64_t size, GLenum usage);
  void bufferData(GLenum target, DOMArrayBufferBase* data, GLenum usage);
  void bufferData(GLenum target,
                  MaybeShared<DOMArrayBufferView> data,
                  GLenum usage);
  void bufferSubData(GLenum target,
                     int64_t offset,
                     base::span<const uint8_t> data);

  GLenum checkFramebufferStatus(GLenum target);
  void clear(GLbitfield mask);
  void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void clearDepth(GLfloat);
  void clearStencil(GLint);
  void colorMask(GLboolean red,
                 GLboolean green,
                 GLboolean blue,
                 GLboolean alpha);
  void compileShader(WebGLShader*);

  void compressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            MaybeShared<DOMArrayBufferView> data);
  void compressedTexSubImage2D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               MaybeShared<DOMArrayBufferView> data);
  void copyTexImage2D(GLenum target,
                      GLint level,
                      GLenum internalformat,
                      GLint x,
                      GLint y,
                      GLsizei width,
                      GLsizei height,
                      GLint border);
  void copyTexSubImage2D(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height);

  WebGLBuffer* createBuffer();
  WebGLFramebuffer* createFramebuffer();
  WebGLProgram* createProgram();
  WebGLRenderbuffer* createRenderbuffer();
  WebGLShader* createShader(GLenum type);
  WebGLTexture* createTexture();

  void cullFace(GLenum mode);

  void deleteBuffer(WebGLBuffer*);
  void deleteFramebuffer(WebGLFramebuffer*);
  void deleteProgram(WebGLProgram*);
  void deleteRenderbuffer(WebGLRenderbuffer*);
  void deleteShader(WebGLShader*);
  void deleteTexture(WebGLTexture*);

  void depthFunc(GLenum);
  void depthMask(GLboolean);
  void depthRange(GLfloat z_near, GLfloat z_far);
  void detachShader(WebGLProgram*, WebGLShader*);
  void disable(GLenum cap);
  void disableVertexAttribArray(GLuint index);
  void drawArrays(GLenum mode, GLint first, GLsizei count);
  void drawElements(GLenum mode, GLsizei count, GLenum type, int64_t offset);

  void enable(GLenum cap);
  void enableVertexAttribArray(GLuint index);
  void finish();
  void flush();
  void framebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               WebGLRenderbuffer*);
  void framebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            WebGLTexture*,
                            GLint level);
  void frontFace(GLenum mode);
  void generateMipmap(GLenum target);

  WebGLActiveInfo* getActiveAttrib(WebGLProgram*, GLuint index);
  WebGLActiveInfo* getActiveUniform(WebGLProgram*, GLuint index);
  std::optional<HeapVector<Member<WebGLShader>>> getAttachedShaders(
      WebGLProgram*);
  GLint getAttribLocation(WebGLProgram*, const String& name);
  ScriptValue getBufferParameter(ScriptState*, GLenum target, GLenum pname);
  WebGLContextAttributes* getContextAttributes() const;
  GLenum getError();
  ScriptObject getExtension(ScriptState*, const String& name);
  ScriptValue getFramebufferAttachmentParameter(ScriptState*,
                                                GLenum target,
                                                GLenum attachment,
                                                GLenum pname);
  ScriptValue getParameter(ScriptState*, GLenum pname);
  ScriptValue getProgramParameter(ScriptState*, WebGLProgram*, GLenum pname);
  String getProgramInfoLog(WebGLProgram*);
  ScriptValue getRenderbufferParameter(ScriptState*,
                                       GLenum target,
                                       GLenum pname);
  ScriptValue getShaderParameter(ScriptState*, WebGLShader*, GLenum pname);
  String getShaderInfoLog(WebGLShader*);
  WebGLShaderPrecisionFormat* getShaderPrecisionFormat(GLenum shader_type,
                                                       GLenum precision_type);
  String getShaderSource(WebGLShader*);
  std::optional<Vector<String>> getSupportedExtensions();
  ScriptValue getTexParameter(ScriptState*, GLenum target, GLenum pname);
  ScriptValue getUniform(ScriptState*,
                         WebGLProgram*,
                         const WebGLUniformLocation*);
  WebGLUniformLocation* getUniformLocation(WebGLProgram*, const String&);
  ScriptValue getVertexAttrib(ScriptState*, GLuint index, GLenum pname);
  int64_t getVertexAttribOffset(GLuint index, GLenum pname);

  void hint(GLenum target, GLenum mode);
  bool isBuffer(WebGLBuffer*);
  bool isEnabled(GLenum cap);
  bool isFramebuffer(WebGLFramebuffer*);
  bool isProgram(WebGLProgram*);
  bool isRenderbuffer(WebGLRenderbuffer*);
  bool isShader(WebGLShader*);
  bool isTexture(WebGLTexture*);

  void lineWidth(GLfloat);
  void linkProgram(WebGLProgram*);
  void pixelStorei(GLenum pname, GLint param);
  void polygonOffset(GLfloat factor, GLfloat units);

  void readPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  MaybeShared<DOMArrayBufferView> pixels);

  void renderbufferStorage(GLenum target,
                           GLenum internalformat,
                           GLsizei width,
                           GLsizei height);

  void sampleCoverage(GLfloat value, GLboolean invert);
  void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
  void shaderSource(WebGLShader*, const String&);
  void stencilFunc(GLenum func, GLint ref, GLuint mask);
  void stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
  void stencilMask(GLuint);
  void stencilMaskSeparate(GLenum face, GLuint mask);
  void stencilOp(GLenum fail, GLenum zfail, GLenum zpass);
  void stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);

  void texParameterf(GLenum target, GLenum pname, GLfloat param);
  void texParameteri(GLenum target, GLenum pname, GLint param);

  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  MaybeShared<DOMArrayBufferView>);
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLenum format,
                  GLenum type,
                  ImageData*);
  void texImage2D(ScriptState*,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLenum format,
                  GLenum type,
                  HTMLImageElement*,
                  ExceptionState&);
  void texImage2D(ScriptState*,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLenum format,
                  GLenum type,
                  CanvasRenderingContextHost*,
                  ExceptionState&);
  void texImage2D(ScriptState*,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLenum format,
                  GLenum type,
                  HTMLVideoElement*,
                  ExceptionState&);
  void texImage2D(ScriptState*,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLenum format,
                  GLenum type,
                  VideoFrame*,
                  ExceptionState&);
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLenum format,
                  GLenum type,
                  ImageBitmap*,
                  ExceptionState&);

  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     MaybeShared<DOMArrayBufferView>);
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLenum format,
                     GLenum type,
                     ImageData*);
  void texSubImage2D(ScriptState*,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLenum format,
                     GLenum type,
                     HTMLImageElement*,
                     ExceptionState&);
  void texSubImage2D(ScriptState*,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLenum format,
                     GLenum type,
                     CanvasRenderingContextHost*,
                     ExceptionState&);
  void texSubImage2D(ScriptState*,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLenum format,
                     GLenum type,
                     HTMLVideoElement*,
                     ExceptionState&);
  void texSubImage2D(ScriptState*,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLenum format,
                     GLenum type,
                     VideoFrame*,
                     ExceptionState&);
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLenum format,
                     GLenum type,
                     ImageBitmap*,
                     ExceptionState&);

  void uniform1f(const WebGLUniformLocation*, GLfloat x);
  void uniform1fv(const WebGLUniformLocation*, base::span<const GLfloat>);
  void uniform1i(const WebGLUniformLocation*, GLint x);
  void uniform1iv(const WebGLUniformLocation*, base::span<const GLint>);
  void uniform2f(const WebGLUniformLocation*, GLfloat x, GLfloat y);
  void uniform2fv(const WebGLUniformLocation*, base::span<const GLfloat>);
  void uniform2i(const WebGLUniformLocation*, GLint x, GLint y);
  void uniform2iv(const WebGLUniformLocation*, base::span<const GLint>);
  void uniform3f(const WebGLUniformLocation*, GLfloat x, GLfloat y, GLfloat z);
  void uniform3fv(const WebGLUniformLocation*, base::span<const GLfloat>);
  void uniform3i(const WebGLUniformLocation*, GLint x, GLint y, GLint z);
  void uniform3iv(const WebGLUniformLocation*, base::span<const GLint>);
  void uniform4f(const WebGLUniformLocation*,
                 GLfloat x,
                 GLfloat y,
                 GLfloat z,
                 GLfloat w);
  void uniform4fv(const WebGLUniformLocation*, base::span<const GLfloat>);
  void uniform4i(const WebGLUniformLocation*,
                 GLint x,
                 GLint y,
                 GLint z,
                 GLint w);
  void uniform4iv(const WebGLUniformLocation*, base::span<const GLint>);

  void uniformMatrix2fv(const WebGLUniformLocation*,
                        GLboolean transpose,
                        base::span<const GLfloat> value);
  void uniformMatrix3fv(const WebGLUniformLocation*,
                        GLboolean transpose,
                        base::span<const GLfloat> value);
  void uniformMatrix4fv(const WebGLUniformLocation*,
                        GLboolean transpose,
                        base::span<const GLfloat> value);

  void useProgram(WebGLProgram*);
  void validateProgram(WebGLProgram*);

  void vertexAttrib1f(GLuint index, GLfloat x);
  void vertexAttrib1fv(GLuint index, base::span<const GLfloat> values);
  void vertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
  void vertexAttrib2fv(GLuint index, base::span<const GLfloat> values);
  void vertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
  void vertexAttrib3fv(GLuint index, base::span<const GLfloat> values);
  void vertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
  void vertexAttrib4fv(GLuint index, base::span<const GLfloat> values);
  void vertexAttribPointer(GLuint index,
                           GLint size,
                           GLenum type,
                           GLboolean normalized,
                           GLsizei stride,
                           int64_t offset);

  void viewport(GLint x, GLint y, GLsizei width, GLsizei height);

  void drawingBufferStorage(GLenum sizedformat, GLsizei width, GLsizei height);

  void commit();

  ScriptPromise<IDLUndefined> makeXRCompatible(ScriptState*, ExceptionState&);
  // **************************************************************************
  // End of WebGLRenderingContextBase's IDL methods
  // **************************************************************************

  // **************************************************************************
  // Start of WebGL2RenderingContextBase's IDL methods
  // **************************************************************************

  /* Buffer objects */
  void bufferData(GLenum target,
                  MaybeShared<DOMArrayBufferView> srcData,
                  GLenum usage,
                  int64_t srcOffset,
                  GLuint length);
  void bufferSubData(GLenum target,
                     int64_t offset,
                     MaybeShared<DOMArrayBufferView> srcData,
                     int64_t srcOffset,
                     GLuint length);
  void copyBufferSubData(GLenum readTarget,
                         GLenum writeTarget,
                         int64_t readOffset,
                         int64_t writeOffset,
                         int64_t size);
  void getBufferSubData(GLenum target,
                        int64_t srcByteOffset,
                        MaybeShared<DOMArrayBufferView> dstData,
                        int64_t dstOffset,
                        GLuint length);

  /* Framebuffer objects */
  void blitFramebuffer(GLint src_x0,
                       GLint src_y0,
                       GLint src_x1,
                       GLint src_y1,
                       GLint dst_x0,
                       GLint dst_y0,
                       GLint dst_x1,
                       GLint dst_y1,
                       GLbitfield mask,
                       GLenum filter);
  void framebufferTextureLayer(GLenum target,
                               GLenum attachment,
                               WebGLTexture* texture,
                               GLint level,
                               GLint layer);
  ScriptValue getInternalformatParameter(ScriptState* script_state,
                                         GLenum target,
                                         GLenum internalformat,
                                         GLenum pname);
  void invalidateFramebuffer(GLenum target, const Vector<GLenum>& attachments);
  void invalidateSubFramebuffer(GLenum target,
                                const Vector<GLenum>& attachments,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height);
  void readBuffer(GLenum mode);

  /* Renderbuffer objects */
  void renderbufferStorageMultisample(GLenum target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height);

  /* Texture objects */
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  int64_t offset);
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  ImageData* pixels);
  void texImage2D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  HTMLImageElement* image,
                  ExceptionState& exception_state);
  // Handles both OffscreenCanvas and HTMLCanvasElement
  void texImage2D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  CanvasRenderingContextHost* canvas,
                  ExceptionState& exception_state);
  void texImage2D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  HTMLVideoElement* video,
                  ExceptionState& exception_state);
  void texImage2D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  VideoFrame* frame,
                  ExceptionState& exception_state);
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  ImageBitmap* bitmap,
                  ExceptionState& exception_state);
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  MaybeShared<DOMArrayBufferView> data,
                  int64_t src_offset);

  void texElementImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLenum format,
                         GLenum type,
                         Element* element,
                         ExceptionState& exception_state);

  void texElementImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         Element* element,
                         ExceptionState& exception_state);

  void texElementImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLfloat sx,
                         GLfloat sy,
                         GLfloat swidth,
                         GLfloat sheight,
                         GLenum format,
                         GLenum type,
                         Element* element,
                         ExceptionState& exception_state);

  void texElementImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLfloat sx,
                         GLfloat sy,
                         GLfloat swidth,
                         GLfloat sheight,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         Element* element,
                         ExceptionState& exception_state);

  void texElement2D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLenum format,
                    GLenum type,
                    Element* element,
                    ExceptionState& exception_state);

  void texElement2D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    Element* element,
                    ExceptionState& exception_state);

  void texElement2D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLfloat sx,
                    GLfloat sy,
                    GLfloat swidth,
                    GLfloat sheight,
                    GLenum format,
                    GLenum type,
                    Element* element,
                    ExceptionState& exception_state);

  void texElement2D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLfloat sx,
                    GLfloat sy,
                    GLfloat swidth,
                    GLfloat sheight,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    Element* element,
                    ExceptionState& exception_state);

  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     int64_t offset);
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     ImageData* pixels);
  void texSubImage2D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     HTMLImageElement* image,
                     ExceptionState& exception_state);
  // Handles both OffscreenCanvas and HTMLCanvasElement
  void texSubImage2D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     CanvasRenderingContextHost* canvas,
                     ExceptionState& exception_state);
  void texSubImage2D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     HTMLVideoElement* video,
                     ExceptionState& exception_state);
  void texSubImage2D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     VideoFrame* frame,
                     ExceptionState& exception_state);
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     ImageBitmap* bitmap,
                     ExceptionState& exception_state);
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     MaybeShared<DOMArrayBufferView> pixels,
                     int64_t src_offset);

  void texStorage2D(GLenum target,
                    GLsizei levels,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height);
  void texStorage3D(GLenum target,
                    GLsizei levels,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth);
  void texImage3D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  int64_t offset);
  void texImage3D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  ImageData* pixels);
  void texImage3D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  HTMLImageElement* image,
                  ExceptionState& exception_state);
  // Handles both OffscreenCanvas and HTMLCanvasElement
  void texImage3D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  CanvasRenderingContextHost* canvas,
                  ExceptionState& exception_state);
  void texImage3D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  HTMLVideoElement* video,
                  ExceptionState& exception_state);
  void texImage3D(ScriptState* script_state,
                  GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  VideoFrame* frame,
                  ExceptionState& exception_state);
  void texImage3D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  ImageBitmap* bitmap,
                  ExceptionState& exception_state);
  void texImage3D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  MaybeShared<DOMArrayBufferView> pixels);
  void texImage3D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  MaybeShared<DOMArrayBufferView> pixels,
                  GLuint src_offset);
  void texSubImage3D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     int64_t offset);
  void texSubImage3D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     ImageData* pixels);
  void texSubImage3D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     HTMLImageElement* image,
                     ExceptionState& exception_state);
  // Handles both OffscreenCanvas and HTMLCanvasElement
  void texSubImage3D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     CanvasRenderingContextHost* context_host,
                     ExceptionState& exception_state);
  void texSubImage3D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     HTMLVideoElement* video,
                     ExceptionState& exception_state);
  void texSubImage3D(ScriptState* script_state,
                     GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     VideoFrame* frame,
                     ExceptionState& exception_state);
  void texSubImage3D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     ImageBitmap* bitmap,
                     ExceptionState& exception_state);
  void texSubImage3D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     MaybeShared<DOMArrayBufferView> pixels);
  void texSubImage3D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLint zoffset,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth,
                     GLenum format,
                     GLenum type,
                     MaybeShared<DOMArrayBufferView> pixels,
                     GLuint src_offset);

  void copyTexSubImage3D(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint zoffset,
                         GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height);

  void compressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            MaybeShared<DOMArrayBufferView> data,
                            GLuint src_offset,
                            GLuint src_length_override);
  void compressedTexSubImage2D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               MaybeShared<DOMArrayBufferView> data,
                               GLuint src_offset,
                               GLuint src_length_override);
  void compressedTexImage3D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLint border,
                            MaybeShared<DOMArrayBufferView> data,
                            GLuint src_offset,
                            GLuint src_length_override);
  void compressedTexSubImage3D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLenum format,
                               MaybeShared<DOMArrayBufferView> data,
                               GLuint src_offset,
                               GLuint src_length_override);
  void compressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLsizei image_size,
                            int64_t offset);
  void compressedTexSubImage2D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLsizei image_size,
                               int64_t offset);
  void compressedTexImage3D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLint border,
                            GLsizei image_size,
                            int64_t offset);
  void compressedTexSubImage3D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLenum format,
                               GLsizei image_size,
                               int64_t offset);

  /* Programs and shaders */
  GLint getFragDataLocation(WebGLProgram* program, const String& name);

  /* Uniforms and attributes */
  void uniform1ui(const WebGLUniformLocation* location, GLuint v0);
  void uniform2ui(const WebGLUniformLocation* location, GLuint v0, GLuint v1);
  void uniform3ui(const WebGLUniformLocation* location,
                  GLuint v0,
                  GLuint v1,
                  GLuint v2);
  void uniform4ui(const WebGLUniformLocation* location,
                  GLuint v0,
                  GLuint v1,
                  GLuint v2,
                  GLuint v3);
  void uniform1fv(const WebGLUniformLocation* location,
                  base::span<const GLfloat> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform2fv(const WebGLUniformLocation* location,
                  base::span<const GLfloat> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform3fv(const WebGLUniformLocation* location,
                  base::span<const GLfloat> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform4fv(const WebGLUniformLocation* location,
                  base::span<const GLfloat> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform1iv(const WebGLUniformLocation* location,
                  base::span<const GLint> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform2iv(const WebGLUniformLocation* location,
                  base::span<const GLint> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform3iv(const WebGLUniformLocation* location,
                  base::span<const GLint> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform4iv(const WebGLUniformLocation* location,
                  base::span<const GLint> v,
                  GLuint src_offset,
                  GLuint src_length);
  void uniform1uiv(const WebGLUniformLocation* location,
                   base::span<const GLuint> v,
                   GLuint src_offset,
                   GLuint src_length);
  void uniform2uiv(const WebGLUniformLocation* location,
                   base::span<const GLuint> v,
                   GLuint src_offset,
                   GLuint src_length);
  void uniform3uiv(const WebGLUniformLocation* location,
                   base::span<const GLuint> v,
                   GLuint src_offset,
                   GLuint src_length);
  void uniform4uiv(const WebGLUniformLocation* location,
                   base::span<const GLuint> v,
                   GLuint src_offset,
                   GLuint src_length);
  void uniformMatrix2fv(const WebGLUniformLocation* location,
                        GLboolean transpose,
                        base::span<const GLfloat> v,
                        GLuint src_offset,
                        GLuint src_length);
  void uniformMatrix3fv(const WebGLUniformLocation* location,
                        GLboolean transpose,
                        base::span<const GLfloat> v,
                        GLuint src_offset,
                        GLuint src_length);
  void uniformMatrix4fv(const WebGLUniformLocation* location,
                        GLboolean transpose,
                        base::span<const GLfloat> v,
                        GLuint src_offset,
                        GLuint src_length);
  void uniformMatrix2x3fv(const WebGLUniformLocation* location,
                          GLboolean transpose,
                          base::span<const GLfloat> v,
                          GLuint src_offset,
                          GLuint src_length);
  void uniformMatrix3x2fv(const WebGLUniformLocation* location,
                          GLboolean transpose,
                          base::span<const GLfloat> v,
                          GLuint src_offset,
                          GLuint src_length);
  void uniformMatrix2x4fv(const WebGLUniformLocation* location,
                          GLboolean transpose,
                          base::span<const GLfloat> v,
                          GLuint src_offset,
                          GLuint src_length);
  void uniformMatrix4x2fv(const WebGLUniformLocation* location,
                          GLboolean transpose,
                          base::span<const GLfloat> v,
                          GLuint src_offset,
                          GLuint src_length);
  void uniformMatrix3x4fv(const WebGLUniformLocation* location,
                          GLboolean transpose,
                          base::span<const GLfloat> v,
                          GLuint src_offset,
                          GLuint src_length);
  void uniformMatrix4x3fv(const WebGLUniformLocation* location,
                          GLboolean transpose,
                          base::span<const GLfloat> v,
                          GLuint src_offset,
                          GLuint src_length);

  void vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
  void vertexAttribI4iv(GLuint index, base::span<const GLint> v);
  void vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
  void vertexAttribI4uiv(GLuint index, base::span<const GLuint> v);
  void vertexAttribIPointer(GLuint index,
                            GLint size,
                            GLenum type,
                            GLsizei stride,
                            int64_t offset);

  /* Writing to the drawing buffer */
  void vertexAttribDivisor(GLuint index, GLuint divisor);
  void drawArraysInstanced(GLenum mode,
                           GLint first,
                           GLsizei count,
                           GLsizei instance_count);
  void drawElementsInstanced(GLenum mode,
                             GLsizei count,
                             GLenum type,
                             int64_t offset,
                             GLsizei instance_count);
  void drawRangeElements(GLenum mode,
                         GLuint start,
                         GLuint end,
                         GLsizei count,
                         GLenum type,
                         int64_t offset);

  /* Multiple Render Targets */
  void drawBuffers(const Vector<GLenum>& buffers);
  void clearBufferiv(GLenum buffer,
                     GLint drawbuffer,
                     base::span<const GLint> value,
                     GLuint src_offset);
  void clearBufferuiv(GLenum buffer,
                      GLint drawbuffer,
                      base::span<const GLuint> value,
                      GLuint src_offset);
  void clearBufferfv(GLenum buffer,
                     GLint drawbuffer,
                     base::span<const GLfloat> value,
                     GLuint src_offset);
  void clearBufferfi(GLenum buffer,
                     GLint drawbuffer,
                     GLfloat depth,
                     GLint stencil);

  /* Query Objects */
  WebGLQuery* createQuery();
  void deleteQuery(WebGLQuery* query);
  bool isQuery(WebGLQuery* query);
  void beginQuery(GLenum target, WebGLQuery* query);
  void endQuery(GLenum target);
  ScriptValue getQuery(ScriptState* script_state, GLenum target, GLenum pname);
  ScriptValue getQueryParameter(ScriptState* script_state,
                                WebGLQuery* query,
                                GLenum pname);

  /* Sampler Objects */
  WebGLSampler* createSampler();
  void deleteSampler(WebGLSampler* sampler);
  bool isSampler(WebGLSampler* sampler);
  void bindSampler(GLuint unit, WebGLSampler* sampler);
  void samplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param);
  void samplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param);
  ScriptValue getSamplerParameter(ScriptState* script_state,
                                  WebGLSampler* sampler,
                                  GLenum pname);

  /* Sync objects */
  WebGLSync* fenceSync(GLenum condition, GLbitfield flags);
  bool isSync(WebGLSync* sync);
  void deleteSync(WebGLSync* sync);
  GLenum clientWaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout);
  void waitSync(WebGLSync* sync, GLbitfield flags, GLint64 timeout);

  ScriptValue getSyncParameter(ScriptState* script_state,
                               WebGLSync* sync,
                               GLenum pname);

  /* Transform Feedback */
  WebGLTransformFeedback* createTransformFeedback();
  void deleteTransformFeedback(WebGLTransformFeedback* feedback);
  bool isTransformFeedback(WebGLTransformFeedback* feedback);
  void bindTransformFeedback(GLenum target, WebGLTransformFeedback* feedback);
  void beginTransformFeedback(GLenum primitive_mode);
  void endTransformFeedback();
  void transformFeedbackVaryings(WebGLProgram* program,
                                 const Vector<String>& varyings,
                                 GLenum buffer_mode);
  WebGLActiveInfo* getTransformFeedbackVarying(WebGLProgram* program,
                                               GLuint index);
  void pauseTransformFeedback();
  void resumeTransformFeedback();

  /* Uniform Buffer Objects and Transform Feedback Buffers */
  void bindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
  void bindBufferRange(GLenum target,
                       GLuint index,
                       WebGLBuffer* buffer,
                       int64_t offset,
                       int64_t size);
  ScriptValue getIndexedParameter(ScriptState* script_state,
                                  GLenum target,
                                  GLuint index);
  std::optional<Vector<GLuint>> getUniformIndices(
      WebGLProgram* program,
      const Vector<String>& uniform_names);
  ScriptValue getActiveUniforms(ScriptState* script_state,
                                WebGLProgram* program,
                                const Vector<GLuint>& uniform_indices,
                                GLenum pname);
  GLuint getUniformBlockIndex(WebGLProgram* program,
                              const String& uniform_block_name);
  ScriptValue getActiveUniformBlockParameter(ScriptState* script_state,
                                             WebGLProgram* program,
                                             GLuint uniform_block_index,
                                             GLenum pname);
  String getActiveUniformBlockName(WebGLProgram* program,
                                   GLuint uniform_block_index);
  void uniformBlockBinding(WebGLProgram* program,
                           GLuint uniform_block_index,
                           GLuint uniform_block_binding);

  /* Vertex Array Objects */
  WebGLVertexArrayObject* createVertexArray();
  void deleteVertexArray(WebGLVertexArrayObject* vertex_array);
  bool isVertexArray(WebGLVertexArrayObject* vertex_array);
  void bindVertexArray(WebGLVertexArrayObject* vertex_array);

  /* Reading */
  void readPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  int64_t offset);
  void readPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  MaybeShared<DOMArrayBufferView> pixels,
                  int64_t offset);

  // **************************************************************************
  // End of WebGL2RenderingContextBase's IDL methods
  // **************************************************************************

  // **************************************************************************
  // Start of CanvasRenderingContext implementation
  // **************************************************************************
  SkAlphaType GetAlphaType() const override;
  viz::SharedImageFormat GetSharedImageFormat() const override;
  gfx::ColorSpace GetColorSpace() const override;
  int AllocatedBufferCountPerPixel() const override;
  bool isContextLost() const override;
  scoped_refptr<StaticBitmapImage> GetImage() override;
  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata) override;

  bool IsComposited() const override;
  bool IsPaintable() const override;
  void PageVisibilityChanged() override;
  scoped_refptr<StaticBitmapImage> PaintRenderingResultsToSnapshot(
      SourceDrawingBuffer source_buffer) override;
  bool CopyRenderingResultsToVideoFrame(
      WebGraphicsContext3DVideoFramePool*,
      SourceDrawingBuffer,
      const gfx::ColorSpace&,
      VideoFrameCopyCompletedCallback) override;

  cc::Layer* CcLayer() const override;
  void Reshape(int width, int height) override;
  void Stop() override;
  void FinalizeFrame(FlushReason) override;
  bool PushFrame() override;

  // **************************************************************************
  // End of CanvasRenderingContext implementation
  // **************************************************************************

  // WebGPUSwapBufferProvider::Client implementation
  void OnTextureTransferred() override;
  void InitializeLayer(cc::Layer* layer) override;
  void SetNeedsCompositingUpdate() override;
  bool IsGPUDeviceDestroyed() override;

  void Trace(Visitor*) const override;

  // Debug message callback from KHR_debug
  void OnDebugMessage(GLenum source,
                      GLenum type,
                      GLuint id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message);

 private:
  void InitRequestAdapterCallback(ScriptState* script_state,
                                  ScriptPromiseResolver<IDLUndefined>* resolver,
                                  wgpu::RequestAdapterStatus status,
                                  wgpu::Adapter adapter,
                                  wgpu::StringView error_message);
  void InitRequestDeviceCallback(ScriptState* script_state,
                                 ScriptPromiseResolver<IDLUndefined>* resolver,
                                 wgpu::RequestDeviceStatus status,
                                 wgpu::Device device,
                                 wgpu::StringView error_message);

  // Must be called when an operation happens that should cause the drawing
  // buffer to be present to the compositor. See WebGL spec Section 2.2 The
  // Drawing Buffer.
  void EnsureDefaultFramebuffer();

  void InitializeContext();
  void Destroy();

  // Validates that the value fits in the positive value of a int32_t to ensure
  // that no clamping occurs when narrowing to GL integer parameters that may be
  // 32 bit integers.
  bool ValidateFitsNonNegInt32(const char* function_name,
                               const char* param_name,
                               int64_t value);

  // Validates that the location is for the current program. Also returns false
  // when the location is null.
  bool ValidateUniformLocation(const char* function_name,
                               const WebGLUniformLocation* location);

  // In addition from ValidateUniformLocation, checks that the size is a
  // multiple of the alignment and fits in a int32_t.
  bool ValidateUniformV(const char* function_name,
                        const WebGLUniformLocation* location,
                        size_t required_size_alignment,
                        size_t size);
  // In addition from ValidateUniformLocation, checks that the range defined by
  // src_offset/size fits in the size, is a multiple of the alignment, and
  // return the corresponding range of data in out.
  template <typename T>
  bool ValidateUniformV(const char* function_name,
                        const WebGLUniformLocation* location,
                        size_t required_size_alignment,
                        base::span<const T> data,
                        GLuint src_offset,
                        GLuint src_size,
                        base::span<const T>* out_data);

  // Helper function for APIs which can legally receive null objects, including
  // the bind* calls (bindBuffer, bindTexture, etc.) and useProgram. Checks that
  // the object belongs to this context and that it's not marked for deletion.
  // Returns false if the caller should return without further processing.
  // This returns true for null WebGLObject arguments!
  bool ValidateNullableObject(const char* function_name,
                              const WebGLObject* object);

  // Validates the incoming WebGL object, which is assumed to be non-null.
  // Checks that the object belongs to this context and that it's not marked for
  // deletion.
  bool ValidateObject(const char* function_name, const WebGLObject* object);

  // Similar to ValidateObject but returns GL_INVALID_VALUE instead of
  // GL_OPERATION_ERROR when a deleted object is used.
  bool ValidateProgramOrShader(const char* function_name,
                               const WebGLObject* object);

  // Helper function for delete* (deleteBuffer, deleteProgram, etc) functions.
  // Return false if caller should return without further processing.
  bool DeleteObject(WebGLObject* object);

  // Clears the current state of had_error_callback_ and returns the previous
  // value. Can be used to clear the state before a critical section and check
  // if an error was generated afterwards.
  bool CheckAndClearErrorCallbackState();

  // Query errors from the driver and populate errors_
  void FlushErrors();

  // Inject an error from the WebGL layer
  void InsertGLError(GLenum error,
                     const char* function_name,
                     const char* description);

  // Print errors and warnings to the console. Errors will stop printing after
  // the num_gl_errors_to_console_allowed_ limit.
  void PrintGLErrorToConsole(const String& message);
  void PrintWarningToConsole(const String& message);

  WebGLFramebuffer* GetBoundFramebuffer(GLenum target) const;

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  std::unique_ptr<gpu::gles2::GLES2Interface> gles2_for_objects_;

  gl::DriverEGL driver_egl_;
  gl::DriverGL driver_gl_;

  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLContext context_ = EGL_NO_CONTEXT;

  scoped_refptr<WebGPUSwapBufferProvider> swap_buffers_;
  wgpu::Texture current_swap_buffer_;
  EGLImage default_framebuffer_color_image_ = EGL_NO_IMAGE;
  GLuint default_framebuffer_color_texture_ = 0;
  GLuint default_framebuffer_depth_stencil_renderbuffer_ = 0;
  gfx::Size default_framebuffer_size_;
  GLuint default_framebuffer_ = 0;

  int num_gl_errors_to_console_allowed_ = 255;
  Vector<GLenum> errors_;
  bool had_error_callback_ = false;

  bool supports_separate_framebuffer_targets_ = false;
  Member<WebGLFramebuffer> draw_framebuffer_binding_;
  Member<WebGLFramebuffer> read_framebuffer_binding_;

  enum class TextureTarget : uint8_t {
    k2D = 0,
    kCubeMap = 1,
    k2DArray = 2,
    k3D = 3,
    k2DMultisample = 4,

    kUnkown = 5,
    kCount = kUnkown,
  };
  static TextureTarget GLenumToTextureTarget(GLenum target);

  // Track the current texture unit to know where to index in bound_textures_
  size_t active_texture_unit_ = 0;

  // Use a limit that is at least ANGLE's IMPLEMENTATION_MAX_ACTIVE_TEXTURES
  // constant
  static constexpr size_t kMaxTextureUnits = 64;
  static constexpr size_t kNumTextureTypes =
      static_cast<size_t>(TextureTarget::kCount);
  std::array<std::array<Member<WebGLTexture>, kMaxTextureUnits>,
             kNumTextureTypes>
      bound_textures_;

  Member<WebGLBuffer> array_buffer_binding_;
  // TODO(413078308): This reference should live in the VAO instead.
  Member<WebGLBuffer> element_array_buffer_binding_;

  Member<WebGLProgram> program_binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_RENDERING_CONTEXT_WEBGPU_BASE_H_
