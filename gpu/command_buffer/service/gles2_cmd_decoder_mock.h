// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the mock GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size.h"

namespace gl {
class GLContext;
class GLSurface;
}

namespace gpu {
class QueryManager;

namespace gles2 {

class ContextGroup;
class ErrorState;
class GpuFenceManager;
struct ContextState;

class MockGLES2Decoder : public GLES2Decoder {
 public:
  MockGLES2Decoder(DecoderClient* client,
                   CommandBufferServiceBase* command_buffer_service,
                   Outputter* outputter);
  ~MockGLES2Decoder() override;

  base::WeakPtr<DecoderContext> AsWeakPtr() override;

  MOCK_METHOD5(Initialize,
               gpu::ContextResult(const scoped_refptr<gl::GLSurface>& surface,
                                  const scoped_refptr<gl::GLContext>& context,
                                  bool offscreen,
                                  const DisallowedFeatures& disallowed_features,
                                  const ContextCreationAttribs& attrib_helper));
  MOCK_METHOD1(Destroy, void(bool have_context));
  MOCK_METHOD1(SetSurface, void(const scoped_refptr<gl::GLSurface>& surface));
  MOCK_METHOD0(ReleaseSurface, void());
  MOCK_METHOD1(TakeFrontBuffer, void(const Mailbox& mailbox));
  MOCK_METHOD2(ReturnFrontBuffer, void(const Mailbox& mailbox, bool is_lost));
  MOCK_METHOD0(GetSavedBackTextureCountForTest, size_t());
  MOCK_METHOD0(GetCreatedBackTextureCountForTest, size_t());
  MOCK_METHOD1(ResizeOffscreenFramebuffer, bool(const gfx::Size& size));
  MOCK_METHOD0(MakeCurrent, bool());
  MOCK_METHOD1(GetServiceIdForTesting, uint32_t(uint32_t client_id));
  MOCK_METHOD0(GetGLES2Util, GLES2Util*());
  MOCK_METHOD0(GetGLSurface, gl::GLSurface*());
  MOCK_METHOD0(GetGLContext, gl::GLContext*());
  MOCK_METHOD0(GetContextGroup, ContextGroup*());
  MOCK_CONST_METHOD0(GetFeatureInfo, const FeatureInfo*());
  MOCK_METHOD0(GetContextState, const ContextState*());
  MOCK_METHOD1(GetTranslator,
               scoped_refptr<ShaderTranslatorInterface>(unsigned int type));
  MOCK_METHOD0(GetCapabilities, Capabilities());
  MOCK_CONST_METHOD0(HasPendingQueries, bool());
  MOCK_METHOD1(ProcessPendingQueries, void(bool));
  MOCK_CONST_METHOD0(HasMoreIdleWork, bool());
  MOCK_METHOD0(PerformIdleWork, void());
  MOCK_CONST_METHOD0(HasPollingWork, bool());
  MOCK_METHOD0(PerformPollingWork, void());
  MOCK_METHOD1(RestoreState, void(const ContextState* prev_state));
  MOCK_CONST_METHOD0(RestoreActiveTexture, void());
  MOCK_CONST_METHOD1(RestoreAllTextureUnitAndSamplerBindings,
                     void(const ContextState* state));
  MOCK_CONST_METHOD1(
      RestoreActiveTextureUnitBinding, void(unsigned int target));
  MOCK_METHOD0(RestoreAllExternalTextureBindingsIfNeeded, void());
  MOCK_METHOD1(RestoreBufferBinding, void(unsigned int target));
  MOCK_CONST_METHOD0(RestoreBufferBindings, void());
  MOCK_CONST_METHOD0(RestoreFramebufferBindings, void());
  MOCK_CONST_METHOD0(RestoreGlobalState, void());
  MOCK_CONST_METHOD0(RestoreProgramBindings, void());
  MOCK_METHOD0(RestoreRenderbufferBindings, void());
  MOCK_METHOD1(RestoreTextureState, void(unsigned service_id));
  MOCK_CONST_METHOD1(RestoreTextureUnitBindings, void(unsigned unit));
  MOCK_METHOD1(RestoreVertexAttribArray, void(unsigned index));
  MOCK_CONST_METHOD0(RestoreDeviceWindowRectangles, void());
  MOCK_CONST_METHOD0(ClearAllAttributes, void());
  MOCK_CONST_METHOD0(RestoreAllAttributes, void());
  MOCK_METHOD0(GetQueryManager, gpu::QueryManager*());
  MOCK_METHOD2(SetQueryCallback, void(unsigned int, base::OnceClosure));
  MOCK_METHOD0(GetGpuFenceManager, gpu::gles2::GpuFenceManager*());
  MOCK_METHOD0(GetFramebufferManager, gpu::gles2::FramebufferManager*());
  MOCK_METHOD0(
      GetTransformFeedbackManager, gpu::gles2::TransformFeedbackManager*());
  MOCK_METHOD0(GetVertexArrayManager, gpu::gles2::VertexArrayManager*());
  MOCK_METHOD0(GetImageManagerForTest, gpu::gles2::ImageManager*());
  MOCK_METHOD1(SetIgnoreCachedStateForTest, void(bool ignore));
  MOCK_METHOD1(SetForceShaderNameHashingForTest, void(bool force));
  MOCK_METHOD1(SetAllowExit, void(bool allow));
  MOCK_METHOD4(DoCommands,
               error::Error(unsigned int num_commands,
                            const volatile void* buffer,
                            int num_entries,
                            int* entries_processed));
  MOCK_METHOD2(GetServiceTextureId,
               bool(uint32_t client_texture_id, uint32_t* service_texture_id));
  MOCK_METHOD9(ClearLevel,
               bool(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int x_offset,
                    int y_offset,
                    int width,
                    int height));
  MOCK_METHOD6(ClearCompressedTextureLevel,
               bool(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    int width,
                    int height));
  MOCK_METHOD1(IsCompressedTextureFormat,
               bool(unsigned format));
  MOCK_METHOD8(ClearLevel3D,
               bool(Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth));
  MOCK_METHOD0(GetErrorState, ErrorState *());
  MOCK_METHOD8(CreateAbstractTexture,
               std::unique_ptr<gpu::gles2::AbstractTexture>(
                   unsigned /* GLenum */ target,
                   unsigned /* GLenum */ internal_format,
                   int /* GLsizei */ width,
                   int /* GLsizei */ height,
                   int /* GLsizei */ depth,
                   int /* GLint */ border,
                   unsigned /* GLenum */ format,
                   unsigned /* GLenum */ type));

  MOCK_METHOD0(GetLogger, Logger*());

  // Workaround for move-only args in GMock.
  MOCK_METHOD1(DoWaitForReadPixels, void(base::OnceClosure* callback));
  void WaitForReadPixels(base::OnceClosure callback) override {
    DoWaitForReadPixels(&callback);
  }

  MOCK_CONST_METHOD0(WasContextLost, bool());
  MOCK_CONST_METHOD0(WasContextLostByRobustnessExtension, bool());
  MOCK_METHOD1(MarkContextLost, void(gpu::error::ContextLostReason reason));
  MOCK_METHOD0(CheckResetStatus, bool());
  MOCK_METHOD4(BindImage,
               void(uint32_t client_texture_id,
                    uint32_t texture_target,
                    gl::GLImage* image,
                    bool can_bind_to_sampler));
  MOCK_METHOD1(
      SetCopyTextureResourceManagerForTest,
      void(CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager));
  MOCK_METHOD1(
      SetCopyTexImageBlitterForTest,
      void(CopyTexImageResourceManager* copy_texture_resource_manager));

 private:
  base::WeakPtrFactory<MockGLES2Decoder> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockGLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_
