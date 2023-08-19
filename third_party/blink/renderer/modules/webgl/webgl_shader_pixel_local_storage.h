// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_SHADER_PIXEL_LOCAL_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_SHADER_PIXEL_LOCAL_STORAGE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/typed_arrays/nadc_typed_array_view.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class WebGLFramebuffer;
class WebGLTexture;

class WebGLShaderPixelLocalStorage final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit WebGLShaderPixelLocalStorage(WebGLRenderingContextBase*);

  WebGLExtensionName GetName() const override;

  GLboolean isCoherent() const;

  void framebufferTexturePixelLocalStorageWEBGL(GLint plane,
                                                WebGLTexture*,
                                                GLint level,
                                                GLint layer);
  void framebufferPixelLocalClearValuefvWEBGL(GLint plane,
                                              NADCTypedArrayView<GLfloat>,
                                              GLuint src_offset);
  void framebufferPixelLocalClearValuefvWEBGL(GLint plane,
                                              const Vector<GLfloat>& value,
                                              GLuint src_offset);
  void framebufferPixelLocalClearValueivWEBGL(GLint plane,
                                              NADCTypedArrayView<GLint>,
                                              GLuint src_offset);
  void framebufferPixelLocalClearValueivWEBGL(GLint plane,
                                              const Vector<GLint>& value,
                                              GLuint src_offset);
  void framebufferPixelLocalClearValueuivWEBGL(GLint plane,
                                               NADCTypedArrayView<GLuint>,
                                               GLuint src_offset);
  void framebufferPixelLocalClearValueuivWEBGL(GLint plane,
                                               const Vector<GLuint>& value,
                                               GLuint src_offset);
  void beginPixelLocalStorageWEBGL(const Vector<GLenum>& loadops);
  void endPixelLocalStorageWEBGL(const Vector<GLenum>& storeops);
  void pixelLocalStorageBarrierWEBGL();
  ScriptValue getFramebufferPixelLocalStorageParameterWEBGL(ScriptState*,
                                                            GLint plane,
                                                            GLenum pname);

 private:
  static WebGLFramebuffer* ValidatePLSFramebuffer(
      WebGLRenderingContextBase* context,
      const char* function_name);
  bool ValidatePLSPlaneIndex(WebGLRenderingContextBase* context,
                             const char* function_name,
                             GLint plane);
  bool ValidatePLSClearCommand(WebGLRenderingContextBase*,
                               const char* function_name,
                               GLint plane,
                               size_t src_length,
                               GLuint src_offset);

  const bool coherent_;
  GLint max_pls_planes_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_SHADER_PIXEL_LOCAL_STORAGE_H_
