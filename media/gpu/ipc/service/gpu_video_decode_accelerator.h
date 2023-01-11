// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/mojo/mojom/gpu_accelerated_video_decoder.mojom.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class GpuDriverBugWorkarounds;
struct GpuPreferences;
}  // namespace gpu

namespace media {

class GpuVideoDecodeAccelerator
    : public VideoDecodeAccelerator::Client,
      public gpu::CommandBufferStub::DestructionObserver {
 public:
  // Each of the arguments to the constructor must outlive this object.
  // |stub->decoder()| will be made current around any operation that touches
  // the underlying VDA so that it can make GL calls safely.
  GpuVideoDecodeAccelerator(
      gpu::CommandBufferStub* stub,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const AndroidOverlayMojoFactoryCB& factory);

  GpuVideoDecodeAccelerator() = delete;
  GpuVideoDecodeAccelerator(const GpuVideoDecodeAccelerator&) = delete;
  GpuVideoDecodeAccelerator& operator=(const GpuVideoDecodeAccelerator&) =
      delete;

  // Static query for the capabilities, which includes the supported profiles.
  // This query calls the appropriate platform-specific version.  The returned
  // capabilities will not contain duplicate supported profile entries.
  static gpu::VideoDecodeAcceleratorCapabilities GetCapabilities(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds);

  // VideoDecodeAccelerator::Client implementation.
  void NotifyInitializationComplete(DecoderStatus status) override;
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;

  // CommandBufferStub::DestructionObserver implementation.
  void OnWillDestroyStub(bool have_context) override;

  // Initialize VDAs from the set of VDAs supported for current platform until
  // one of them succeeds for given |config|. Send the |init_done_msg| when
  // done. filter_ is passed to gpu::CommandBufferStub channel only if the
  // chosen VDA can decode on IO thread.
  bool Initialize(
      const VideoDecodeAccelerator::Config& config,
      mojo::PendingAssociatedReceiver<mojom::GpuAcceleratedVideoDecoder>
          receiver,
      mojo::PendingAssociatedRemote<mojom::GpuAcceleratedVideoDecoderClient>
          client);

 private:
  class MessageFilter;

  // We only allow self-delete, from DeleteSelfNow().
  ~GpuVideoDecodeAccelerator() override;

  void DeleteSelfNow();

  // Handlers for IPC messages.
  void OnDecode(BitstreamBuffer bitstream_buffer);
  void OnAssignPictureBuffers(
      std::vector<mojom::PictureBufferAssignmentPtr> assignments);
  void OnReusePictureBuffer(int32_t picture_buffer_id);
  void OnFlush(base::OnceClosure callback);
  void OnReset(base::OnceClosure callback);
  void OnSetOverlayInfo(const OverlayInfo& overlay_info);
  void OnDestroy();

  // Sets the texture to cleared.
  void SetTextureCleared(const Picture& picture);

  // OpenGL Callback methods.
  GpuVideoDecodeGLClient gl_client_;

  // Unowned pointer to the underlying gpu::CommandBufferStub.  |this| is
  // registered as a DestructionObserver of |stub_| and will self-delete when
  // |stub_| is destroyed.
  const raw_ptr<gpu::CommandBufferStub> stub_;

  // The underlying VideoDecodeAccelerator.
  std::unique_ptr<VideoDecodeAccelerator> video_decode_accelerator_;

  // An interface back to the client of this decoder.
  mojo::SharedAssociatedRemote<mojom::GpuAcceleratedVideoDecoderClient>
      decoder_client_;

  // Pending replies to Flush and Reset operations.
  base::circular_deque<base::OnceClosure> pending_flushes_;
  base::circular_deque<base::OnceClosure> pending_resets_;

  // The texture dimensions as requested by ProvidePictureBuffers().
  gfx::Size texture_dimensions_;

  // The texture target as requested by ProvidePictureBuffers().
  uint32_t texture_target_;

  // The format of the picture buffers requested by ProvidePictureBuffers().
  VideoPixelFormat pixel_format_;

  // The number of textures per picture buffer as requested by
  // ProvidePictureBuffers().
  uint32_t textures_per_buffer_;

  // The message filter to run VDA::Decode on IO thread if VDA supports it.
  std::unique_ptr<MessageFilter> filter_;

  // GPU child thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Optional factory for mojo-based android overlays.
  AndroidOverlayMojoFactoryCB overlay_factory_cb_;

  // Weak pointers will be invalidated on IO thread.
  base::WeakPtrFactory<Client> weak_factory_for_io_{this};

  // Protects |uncleared_textures_| when DCHECK is on. This is for debugging
  // only. We don't want to hold a lock on IO thread. When DCHECK is off,
  // |uncleared_textures_| is only accessed from the child thread.
  base::Lock debug_uncleared_textures_lock_;

  // A map from picture buffer ID to set of TextureRefs that have not been
  // cleared.
  std::map<int32_t, std::vector<scoped_refptr<gpu::gles2::TextureRef>>>
      uncleared_textures_;
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_GPU_VIDEO_DECODE_ACCELERATOR_H_
