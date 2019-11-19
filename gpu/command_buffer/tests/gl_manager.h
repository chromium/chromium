// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gl {

class GLContext;
class GLShareGroup;
class GLSurface;

}

namespace gpu {

class CommandBufferDirect;
class GpuMemoryBufferFactory;
class ImageFactory;
class MailboxManager;
class TransferBuffer;

namespace gles2 {

class GLES2CmdHelper;
class GLES2Implementation;

}  // namespace gles2

class GLManager : private GpuControl {
 public:
  struct Options {
    Options();
    // The size of the backbuffer.
    gfx::Size size = gfx::Size(4, 4);
    // If not null will share resources with this context.
    GLManager* share_group_manager = nullptr;
    // If not null will share a mailbox manager with this context.
    GLManager* share_mailbox_manager = nullptr;
    // If not null will create a virtual manager based on this context.
    GLManager* virtual_manager = nullptr;
    // Whether or not glBindXXX generates a resource.
    bool bind_generates_resource = false;
    // Whether or not the context is auto-lost when GL_OUT_OF_MEMORY occurs.
    bool lose_context_when_out_of_memory = false;
    // Whether or not it's ok to lose the context.
    bool context_lost_allowed = false;
    ContextType context_type = CONTEXT_TYPE_OPENGLES2;
    // Force shader name hashing for all context types.
    bool force_shader_name_hashing = false;
    // Whether the buffer is multisampled.
    bool multisampled = false;
    // Whether the backbuffer has an alpha channel.
    bool backbuffer_alpha = true;
    // The ImageFactory to use to generate images for the backbuffer.
    gpu::ImageFactory* image_factory = nullptr;
    // Whether to preserve the backbuffer after a call to SwapBuffers().
    bool preserve_backbuffer = false;
    // Shared memory limits
    SharedMemoryLimits shared_memory_limits = {};
  };
  GLManager();
  ~GLManager() override;

  // GPU feature info computed for the platform.
  // Each test needs to apply them, plus the specific settings a test wants
  // to test.
  static GpuFeatureInfo g_gpu_feature_info;

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format);

  void Initialize(const Options& options);
  void InitializeWithWorkarounds(const Options& options,
                                 const GpuDriverBugWorkarounds& workarounds);
  void Destroy();

  bool IsInitialized() const { return gles2_implementation() != nullptr; }

  void MakeCurrent();

  void SetSurface(gl::GLSurface* surface);

  void PerformIdleWork();

  void set_use_iosurface_memory_buffers(bool use_iosurface_memory_buffers) {
    use_iosurface_memory_buffers_ = use_iosurface_memory_buffers;
  }

  void set_use_native_pixmap_memory_buffers(
      bool use_native_pixmap_memory_buffers) {
    use_native_pixmap_memory_buffers_ = use_native_pixmap_memory_buffers;
  }

  gles2::GLES2Decoder* decoder() const {
    return decoder_.get();
  }

  MailboxManager* mailbox_manager() const { return mailbox_manager_; }

  gl::GLShareGroup* share_group() const { return share_group_.get(); }

  gles2::GLES2Implementation* gles2_implementation() const {
    return gles2_implementation_.get();
  }

  gl::GLContext* context() { return context_.get(); }

  ServiceDiscardableManager* discardable_manager() {
    return &discardable_manager_;
  }
  PassthroughDiscardableManager* passthrough_discardable_manager() {
    return &passthrough_discardable_manager_;
  }

  const GpuDriverBugWorkarounds& workarounds() const;
  const gpu::GpuPreferences& gpu_preferences() const {
    return gpu_preferences_;
  }

  // GpuControl implementation.
  void SetGpuControlClient(GpuControlClient*) override;
  const Capabilities& GetCapabilities() const override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height) override;
  void DestroyImage(int32_t id) override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  CommandBufferId GetCommandBufferID() const override;
  void FlushPendingWork() override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncReleased(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) override;
  void SetDisplayTransform(gfx::OverlayTransform transform) override;

  size_t GetSharedMemoryBytesAllocated() const;
  ContextType GetContextType() const;

  void Reset();

 private:
  void SetupBaseContext();

  void InitializeWithWorkaroundsImpl(
      const Options& options,
      const GpuDriverBugWorkarounds& workarounds);

  gpu::GpuPreferences gpu_preferences_;

  gles2::MailboxManagerImpl owned_mailbox_manager_;
  gles2::TraceOutputter outputter_;
  gles2::ImageManager image_manager_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
  std::unique_ptr<gles2::ShaderTranslatorCache> translator_cache_;
  gles2::FramebufferCompletenessCache completeness_cache_;
  MailboxManager* mailbox_manager_ = nullptr;
  scoped_refptr<gl::GLShareGroup> share_group_;
  std::unique_ptr<CommandBufferDirect> command_buffer_;
  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  std::unique_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<gles2::GLES2Implementation> gles2_implementation_;
  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
  SharedImageManager shared_image_manager_;

  bool use_iosurface_memory_buffers_ = false;
  bool use_native_pixmap_memory_buffers_ = false;

  Capabilities capabilities_;

  // Used on Android to virtualize GL for all contexts.
  static int use_count_;
  static scoped_refptr<gl::GLShareGroup>* base_share_group_;
  static scoped_refptr<gl::GLSurface>* base_surface_;
  static scoped_refptr<gl::GLContext>* base_context_;
  ContextType context_type_ = CONTEXT_TYPE_OPENGLES2;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_MANAGER_H_
