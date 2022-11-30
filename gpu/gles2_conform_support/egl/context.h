// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_CONTEXT_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_CONTEXT_H_

#include <memory>

#include <EGL/egl.h>
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
class ServiceDiscardableManager;
class TransferBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu

namespace gles2_conform_support {
namespace egl {
class Display;
class Surface;
class Config;

class Context : public base::RefCountedThreadSafe<Context>,
                public gpu::GpuControl {
 public:
  Context(Display* display, const Config* config);

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  bool is_current_in_some_thread() const { return is_current_in_some_thread_; }
  void set_is_current_in_some_thread(bool flag) {
    is_current_in_some_thread_ = flag;
  }
  void MarkDestroyed();
  bool SwapBuffers(Surface* current_surface);

  static bool MakeCurrent(Context* current_context,
                          Surface* current_surface,
                          Context* new_context,
                          Surface* new_surface);

  static bool ValidateAttributeList(const EGLint* attrib_list);

  // GpuControl implementation.
  void SetGpuControlClient(gpu::GpuControlClient*) override;
  const gpu::Capabilities& GetCapabilities() const override;
  void SignalQuery(uint32_t query, base::OnceClosure callback) override;
  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override;
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  void FlushPendingWork() override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncReleased(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) override;

  // Called by ThreadState to set the needed global variables when this context
  // is current.
  void ApplyCurrentContext(gl::GLSurface* current_surface);
  static void ApplyContextReleased();

  static void SetPlatformGpuFeatureInfo(
      const gpu::GpuFeatureInfo& gpu_feature_info);

 private:
  friend class base::RefCountedThreadSafe<Context>;
  ~Context() override;
  bool CreateService(gl::GLSurface* gl_surface);
  void DestroyService();
  // Returns true if the object has GL service, either a working one or one
  // that has lost its GL context.
  bool HasService() const;
  void MarkServiceContextLost();
  bool WasServiceContextLost() const;
  bool IsCompatibleSurface(Surface* surface) const;
  bool Flush(gl::GLSurface* gl_surface);

  static gpu::GpuFeatureInfo platform_gpu_feature_info_;

  raw_ptr<Display> display_;
  raw_ptr<const Config> config_;
  bool is_current_in_some_thread_;
  bool is_destroyed_;
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  std::unique_ptr<gpu::CommandBufferDirect> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_cmd_helper_;

  gpu::gles2::MailboxManagerImpl mailbox_manager_;
  gpu::gles2::TraceOutputter outputter_;
  gpu::ServiceDiscardableManager discardable_manager_;
  gpu::PassthroughDiscardableManager passthrough_discardable_manager_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::gles2::ShaderTranslatorCache translator_cache_;
  gpu::gles2::FramebufferCompletenessCache completeness_cache_;
  std::unique_ptr<gpu::gles2::GLES2Decoder> decoder_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;

  scoped_refptr<gl::GLContext> gl_context_;

  std::unique_ptr<gpu::gles2::GLES2Interface> client_gl_context_;

  gpu::Capabilities capabilities_;
};

}  // namespace egl
}  // namespace gles2_conform_support

#endif  // GPU_GLES2_CONFORM_SUPPORT_EGL_CONTEXT_H_
