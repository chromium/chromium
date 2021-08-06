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
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_image.h"

namespace gfx {
class Size;
}

namespace gpu {
class GpuChannel;
struct Mailbox;

// This class is thread safe to be used by multiple gpu threads as
// |texture_owner_| is thread safe and all other members are only accessed on
// gpu main thread.
class StreamTexture : public StreamTextureSharedImageInterface,
                      public mojom::StreamTexture {
 public:
  static scoped_refptr<StreamTexture> Create(
      GpuChannel* channel,
      int stream_id,
      mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver);

  // Cleans up related data and nulls |channel_|. Called when the channel
  // releases its ref on this class.
  void ReleaseChannel();

 private:
  StreamTexture(GpuChannel* channel,
                int32_t route_id,
                mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver,
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

  // gpu::StreamTextureSharedImageInterface implementation.
  void ReleaseResources() override {}
  bool IsUsingGpuMemory() const override;
  void UpdateAndBindTexImage(GLuint service_id) override;
  bool HasTextureOwner() const override;
  TextureBase* GetTextureBase() const override;
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
  bool RenderToOverlay() override;
  bool TextureOwnerBindsTextureOnUpdate() override;

  gpu::Mailbox CreateSharedImage(const gfx::Size& coded_size);

  // Called when a new frame is available for the SurfaceOwner.
  void OnFrameAvailable();

  // mojom::StreamTexture:
  void ForwardForSurfaceRequest(const base::UnguessableToken& token) override;
  void StartListening(mojo::PendingAssociatedRemote<mojom::StreamTextureClient>
                          client) override;
  void UpdateRotatedVisibleSize(const gfx::Size& natural_size) override;

  // The TextureOwner which receives frames.
  scoped_refptr<TextureOwner> texture_owner_;

  // Current visible size from media player, includes rotation.
  gfx::Size rotated_visible_size_;

  // Whether a new frame is available that we should update to.
  bool has_pending_frame_;

  GpuChannel* channel_;
  const int32_t route_id_;
  scoped_refptr<SharedContextState> context_state_;
  SequenceId sequence_;

  mojo::AssociatedReceiver<mojom::StreamTexture> receiver_;
  mojo::AssociatedRemote<mojom::StreamTextureClient> client_;

  gfx::Size coded_size_;
  gfx::Rect visible_rect_;

  // Bound to the thread on which StreamTexture is created. Some methods can
  // only be called on this thread. StreamTexture is created on gpu main thread.
  THREAD_CHECKER(gpu_main_thread_checker_);

  base::WeakPtrFactory<StreamTexture> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(StreamTexture);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_STREAM_TEXTURE_ANDROID_H_
