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

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GLSurface;
}

namespace gpu {

class DecoderClient;
struct Mailbox;

namespace gles2 {

class ContextGroup;
class CopyTexImageResourceManager;
class CopyTextureCHROMIUMResourceManager;
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
    return !std::memcmp(this, &other, sizeof(*this));
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
  static GLES2Decoder* Create(DecoderClient* client,
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

  // Set the surface associated with the default FBO.
  virtual void SetSurface(const scoped_refptr<gl::GLSurface>& surface) = 0;
  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  virtual void ReleaseSurface() = 0;

  // This is intended only for use with NaCL swapchain, replacing
  // TakeFrontBuffer/ReturnFrontBuffer flow.
  virtual void SetDefaultFramebufferSharedImage(const Mailbox& mailbox,
                                                int samples,
                                                bool preserve,
                                                bool needs_depth,
                                                bool needs_stencil) = 0;

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
