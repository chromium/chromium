// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_

#include <stdint.h>

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GLSurface;
}

namespace gpu {

class DecoderClient;

namespace gles2 {

class ContextGroup;
class CopyTexImageResourceManager;
class CopyTextureCHROMIUMResourceManager;
struct DisallowedFeatures;
class FramebufferManager;
class GLES2Util;
class Logger;
class Outputter;
class ShaderTranslatorInterface;
class TransformFeedbackManager;
class VertexArrayManager;

struct GPU_GLES2_EXPORT DisallowedFeatures {
  DisallowedFeatures();
  ~DisallowedFeatures();
  DisallowedFeatures(const DisallowedFeatures&);

  void AllowExtensions() {
    chromium_color_buffer_float_rgba = false;
    chromium_color_buffer_float_rgb = false;
    ext_color_buffer_float = false;
    ext_color_buffer_half_float = false;
    ext_texture_filter_anisotropic = false;
    oes_texture_float_linear = false;
    oes_texture_half_float_linear = false;
    ext_float_blend = false;
    oes_fbo_render_mipmap = false;
    oes_draw_buffers_indexed = false;
  }

  bool operator==(const DisallowedFeatures& other) const {
    return !UNSAFE_TODO(std::memcmp(this, &other, sizeof(*this)));
  }

  bool npot_support = false;
  bool chromium_color_buffer_float_rgba = false;
  bool chromium_color_buffer_float_rgb = false;
  bool ext_color_buffer_float = false;
  bool ext_color_buffer_half_float = false;
  bool ext_texture_filter_anisotropic = false;
  bool oes_texture_float_linear = false;
  bool oes_texture_half_float_linear = false;
  bool ext_float_blend = false;
  bool oes_fbo_render_mipmap = false;
  bool oes_draw_buffers_indexed = false;
};

// This class implements the DecoderContext interface, decoding GLES2
// commands and calling GL.
class GPU_GLES2_EXPORT GLES2Decoder : public CommonDecoder,
                                      public DecoderContext {
 public:
  typedef error::Error Error;

  // The default stencil mask, which has all bits set.  This really should be a
  // GLuint, but we can't #include gl_bindings.h in this file without causing
  // macro redefinitions.
  static const unsigned int kDefaultStencilMask;

  // Creates a decoder.
  static std::unique_ptr<GLES2Decoder> Create(
      DecoderClient* client,
      CommandBufferServiceBase* command_buffer_service,
      Outputter* outputter,
      ContextGroup* group);

  GLES2Decoder(const GLES2Decoder&) = delete;
  GLES2Decoder& operator=(const GLES2Decoder&) = delete;

  ~GLES2Decoder() override;

  // DecoderContext implementation.
  bool initialized() const override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override;
  void BeginDecoding() override;
  void EndDecoding() override;
  std::string_view GetLogPrefix() override;

  void set_initialized() {
    initialized_ = true;
  }

  bool debug() const {
    return debug_;
  }

  // Set to true to call glGetError after every command.
  void set_debug(bool debug) {
    debug_ = debug;
  }

  bool log_commands() const {
    return log_commands_;
  }

  // Set to true to LOG every command.
  void SetLogCommands(bool log_commands) override;

  Outputter* outputter() const override;

  int GetRasterDecoderId() const override;

  // Initializes the graphics context. Can create an offscreen
  // decoder with a frame buffer that can be referenced from the parent.
  // Takes ownership of GLContext.
  // Parameters:
  //  surface: the GL surface to render to.
  //  context: the GL context to render to.
  //  offscreen: whether to make the context offscreen or not. When FBO 0 is
  //      bound, offscreen contexts render to an internal buffer, onscreen ones
  //      to the surface.
  //  context_type: type of the command buffer, should be WEBGL1, WEBGL2 or
  //      OPENGLES2.
  //  lose_context_when_out_of_memory: lose this context in case of out of
  //      memory errors.
  // Returns:
  //   true if successful.
  virtual gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      ContextType context_type,
      bool lose_context_when_out_of_memory) = 0;

  // Set the surface associated with the default FBO.
  virtual void SetSurface(const scoped_refptr<gl::GLSurface>& surface) = 0;
  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  virtual void ReleaseSurface() = 0;

  // Gets the GLES2 Util which holds info.
  virtual GLES2Util* GetGLES2Util() = 0;

  // Restore States.
  virtual void RestoreDeviceWindowRectangles() const = 0;

  virtual void SetIgnoreCachedStateForTest(bool ignore) = 0;
  virtual void SetForceShaderNameHashingForTest(bool force) = 0;
  virtual uint32_t GetAndClearBackbufferClearBitsForTest();

  // Gets the FramebufferManager for this context.
  virtual FramebufferManager* GetFramebufferManager() = 0;

  // Gets the TransformFeedbackManager for this context.
  virtual TransformFeedbackManager* GetTransformFeedbackManager() = 0;

  // Gets the VertexArrayManager for this context.
  virtual VertexArrayManager* GetVertexArrayManager() = 0;

  // Get the service texture ID corresponding to a client texture ID.
  // If no such record is found then return false.
  virtual bool GetServiceTextureId(uint32_t client_texture_id,
                                   uint32_t* service_texture_id);

  virtual void WaitForReadPixels(base::OnceClosure callback) = 0;

  virtual Logger* GetLogger() = 0;

  virtual scoped_refptr<ShaderTranslatorInterface> GetTranslator(
      unsigned int type) = 0;

  virtual void SetCopyTextureResourceManagerForTest(
      CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager) = 0;

  virtual void SetCopyTexImageBlitterForTest(
      CopyTexImageResourceManager* copy_tex_image_blit) = 0;

 protected:
  GLES2Decoder(DecoderClient* client,
               CommandBufferServiceBase* command_buffer_service,
               Outputter* outputter);

 private:
  bool initialized_ = false;
  bool debug_ = false;
  bool log_commands_ = false;
  raw_ptr<Outputter> outputter_ = nullptr;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
