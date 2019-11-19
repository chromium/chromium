// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_STREAM_TEXTURE_ANDROID_H_
#define GPU_IPC_SERVICE_STREAM_TEXTURE_ANDROID_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "ipc/ipc_listener.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_image.h"

namespace gfx {
class Size;
}

namespace gpu {
class GpuChannel;
struct Mailbox;

class StreamTexture : public StreamTextureSharedImageInterface,
                      public IPC::Listener,
                      public SharedContextState::ContextLostObserver {
 public:
  static scoped_refptr<StreamTexture> Create(GpuChannel* channel,
                                             int stream_id);

  // Cleans up related data and nulls |channel_|. Called when the channel
  // releases its ref on this class.
  void ReleaseChannel();

 private:
  StreamTexture(GpuChannel* channel,
                int32_t route_id,
                scoped_refptr<SharedContextState> context_state);
  ~StreamTexture() override;

  // Static function which is used to access |weak_stream_texture| on correct
  // thread since WeakPtr is not thread safe.
  static void RunCallback(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<StreamTexture> weak_stream_texture);

  // gl::GLImage implementation:
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  bool HasMutableState() const override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;

  // gpu::gles2::GLStreamTextureMatrix implementation
  void GetTextureMatrix(float xform[16]) override;
  void NotifyPromotionHint(bool promotion_hint,
                           int display_x,
                           int display_y,
                           int display_width,
                           int display_height) override;

  // gpu::StreamTextureSharedImageInterface implementation.
  void ReleaseResources() override {}
  bool IsUsingGpuMemory() const override;
  void UpdateAndBindTexImage() override;
  bool HasTextureOwner() const override;
  gles2::Texture* GetTexture() const override;
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
  bool RenderToOverlay() override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

  void UpdateTexImage(BindingsMode bindings_mode);
  void EnsureBoundIfNeeded(BindingsMode mode);

  // Called when a new frame is available for the SurfaceOwner.
  void OnFrameAvailable();

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC message handlers:
  void OnStartListening();
  void OnForwardForSurfaceRequest(const base::UnguessableToken& request_token);
  void OnCreateSharedImage(const gpu::Mailbox& mailbox,
                           const gfx::Size& size,
                           uint32_t release_id);
  void OnDestroy();

  // The TextureOwner which receives frames.
  scoped_refptr<TextureOwner> texture_owner_;

  // Current transform matrix of the surface owner.
  float current_matrix_[16];

  // Current size of the surface owner.
  gfx::Size size_;

  // Whether a new frame is available that we should update to.
  bool has_pending_frame_;

  GpuChannel* channel_;
  int32_t route_id_;
  bool has_listener_;
  scoped_refptr<SharedContextState> context_state_;
  SequenceId sequence_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;

  // This indicates whether ycbcr info is already sent from gpu process to the
  // renderer.
  bool ycbcr_info_sent_ = false;

  base::WeakPtrFactory<StreamTexture> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(StreamTexture);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_STREAM_TEXTURE_ANDROID_H_
