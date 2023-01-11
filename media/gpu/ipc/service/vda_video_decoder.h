// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_VDA_VIDEO_DECODER_H_
#define MEDIA_GPU_IPC_SERVICE_VDA_VIDEO_DECODER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/lru_cache.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/ipc/service/picture_buffer_manager.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class CommandBufferStub;
class GpuDriverBugWorkarounds;
struct GpuPreferences;
}  // namespace gpu

namespace media {

// Implements the VideoDecoder interface backed by a VideoDecodeAccelerator.
// This class expects to run in the GPU process via MojoVideoDecoder.
class VdaVideoDecoder : public VideoDecoder,
                        public VideoDecodeAccelerator::Client {
 public:
  using GetStubCB = base::RepeatingCallback<gpu::CommandBufferStub*()>;
  using CreatePictureBufferManagerCB =
      base::OnceCallback<scoped_refptr<PictureBufferManager>(
          PictureBufferManager::ReusePictureBufferCB)>;
  using CreateCommandBufferHelperCB =
      base::OnceCallback<scoped_refptr<CommandBufferHelper>()>;
  using CreateAndInitializeVdaCB =
      base::RepeatingCallback<std::unique_ptr<VideoDecodeAccelerator>(
          scoped_refptr<CommandBufferHelper>,
          VideoDecodeAccelerator::Client*,
          MediaLog*,
          const VideoDecodeAccelerator::Config&)>;
  using GetVdaCapabilitiesCB =
      base::OnceCallback<VideoDecodeAccelerator::Capabilities(
          const gpu::GpuPreferences&,
          const gpu::GpuDriverBugWorkarounds&)>;

  // Creates a VdaVideoDecoder. The returned unique_ptr can be safely upcast to
  // unique_ptr<VideoDecoder>.
  //
  // |get_stub_cb|: Callback to retrieve the CommandBufferStub that should be
  //     used for allocating textures and mailboxes. This is only used when
  //     |output_mode| is ALLOCATE. This callback will be called on the GPU
  //     thread.
  //
  // See VdaVideoDecoder() for other arguments.
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      const gfx::ColorSpace& target_color_space,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      GetStubCB get_stub_cb,
      VideoDecodeAccelerator::Config::OutputMode output_mode);

  VdaVideoDecoder(const VdaVideoDecoder&) = delete;
  VdaVideoDecoder& operator=(const VdaVideoDecoder&) = delete;
  ~VdaVideoDecoder() override;
  static void DestroyAsync(std::unique_ptr<VdaVideoDecoder>);

  // media::VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  bool FramesHoldExternalResources() const override;

 private:
  friend class VdaVideoDecoderTest;

  // |parent_task_runner|: Task runner that |this| should operate on. All
  //     methods must be called on |parent_task_runner| (should be the Mojo
  //     MediaService task runner).
  // |gpu_task_runner|: Task runner that GPU command buffer methods must be
  //     called on (should be the GPU main thread).
  // |media_log|: MediaLog object to log to.
  // |target_color_space|: Color space of the output device.
  // |create_picture_buffer_manager_cb|: PictureBufferManager factory.
  // |create_command_buffer_helper_cb|: CommandBufferHelper factory. This is
  //     only used when |output_mode| is ALLOCATE.
  // |create_and_initialize_vda_cb|: VideoDecodeAccelerator factory.
  // |vda_capabilities|: Capabilities of the VDA that
  //     |create_and_initialize_vda_cb| will produce.
  // |output_mode|: How to manage memory for output frames:
  //     - ALLOCATE: output buffer allocation is expected to be done by a
  //     combination of the PictureBufferManager (for texture allocation,
  //     possibly) and the VDA when AssignPictureBuffers() is called. In this
  //     case, the VdaVideoDecoder will output Mailbox-backed VideoFrames.
  //     - IMPORT, output buffer allocation is done by the PictureBufferManager
  //     (to allocate GpuMemoryBuffers without textures) and these buffers are
  //     imported into the VDA by calling ImportBufferForPicture(). In this
  //     case, the VdaVideoDecoder will output GpuMemoryBuffer-backed
  //     VideoFrames.
  VdaVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      const gfx::ColorSpace& target_color_space,
      CreatePictureBufferManagerCB create_picture_buffer_manager_cb,
      CreateCommandBufferHelperCB create_command_buffer_helper_cb,
      CreateAndInitializeVdaCB create_and_initialize_vda_cb,
      const VideoDecodeAccelerator::Capabilities& vda_capabilities,
      VideoDecodeAccelerator::Config::OutputMode output_mode);

  // media::VideoDecodeAccelerator::Client implementation.
  void NotifyInitializationComplete(DecoderStatus status) override;
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override;
  void ProvidePictureBuffersWithVisibleRect(uint32_t requested_num_of_buffers,
                                            VideoPixelFormat format,
                                            uint32_t textures_per_buffer,
                                            const gfx::Size& dimensions,
                                            const gfx::Rect& visible_rect,
                                            uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;
  gpu::SharedImageStub* GetSharedImageStub() const override;
  CommandBufferHelper* GetCommandBufferHelper() const override;

  // Tasks and thread hopping.
  static void CleanupOnGpuThread(std::unique_ptr<VdaVideoDecoder>);
  void InitializeOnGpuThread();
  void ReinitializeOnGpuThread();
  void InitializeDone(DecoderStatus status);
  void DecodeOnGpuThread(scoped_refptr<DecoderBuffer> buffer,
                         int32_t bitstream_id);
  void DismissPictureBufferOnParentThread(int32_t picture_buffer_id);
  void PictureReadyOnParentThread(Picture picture);
  void NotifyEndOfBitstreamBufferOnParentThread(int32_t bitstream_buffer_id);
  void NotifyFlushDoneOnParentThread();
  void NotifyResetDoneOnParentThread();
  void NotifyErrorOnParentThread(VideoDecodeAccelerator::Error error);
  void ProvidePictureBuffersAsync(uint32_t count,
                                  VideoPixelFormat pixel_format,
                                  uint32_t planes,
                                  gfx::Size texture_size,
                                  GLenum texture_target);
  void ReusePictureBuffer(int32_t picture_buffer_id);

  // Error handling.
  void EnterErrorState();
  void DestroyCallbacks();

  //
  // Construction parameters.
  //
  scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  std::unique_ptr<MediaLog> media_log_;
  gfx::ColorSpace target_color_space_;
  scoped_refptr<PictureBufferManager> picture_buffer_manager_;
  CreateCommandBufferHelperCB create_command_buffer_helper_cb_;
  CreateAndInitializeVdaCB create_and_initialize_vda_cb_;
  const VideoDecodeAccelerator::Capabilities vda_capabilities_;
  const VideoDecodeAccelerator::Config::OutputMode output_mode_;

  //
  // Parent thread state.
  //
  bool has_error_ = false;

  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB flush_cb_;
  base::OnceClosure reset_cb_;

  int32_t bitstream_buffer_id_ = 0;
  std::map<int32_t, DecodeCB> decode_cbs_;
  // Records timestamps so that they can be mapped to output pictures. Must be
  // large enough to account for any amount of frame reordering.
  base::LRUCache<int32_t, base::TimeDelta> timestamps_;

  //
  // Shared state.
  //

  // Only read on GPU thread during initialization, which is mutually exclusive
  // with writes on the parent thread.
  VideoDecoderConfig config_;

  // Only written on the GPU thread during initialization, which is mutually
  // exclusive with reads on the parent thread.
  std::unique_ptr<VideoDecodeAccelerator> vda_;
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;
  bool vda_initialized_ = false;
  bool decode_on_parent_thread_ = false;
  bool reinitializing_ = false;

  //
  // Weak pointers, prefixed by bound thread.
  //
  // |gpu_weak_vda_| is invalidated when the VDA has notified about an error, or
  //     has been destroyed. It is not valid to call VDA methods in those cases.
  base::WeakPtr<VideoDecodeAccelerator> gpu_weak_vda_;
  std::unique_ptr<base::WeakPtrFactory<VideoDecodeAccelerator>>
      gpu_weak_vda_factory_;

  // |gpu_weak_this_| is never explicitly invalidated.
  // |parent_weak_this_| is invalidated when the client calls Destroy(), and
  //     indicates that we should not make any new client callbacks.
  base::WeakPtr<VdaVideoDecoder> gpu_weak_this_;
  base::WeakPtr<VdaVideoDecoder> parent_weak_this_;
  base::WeakPtrFactory<VdaVideoDecoder> gpu_weak_this_factory_{this};
  base::WeakPtrFactory<VdaVideoDecoder> parent_weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_VDA_VIDEO_DECODER_H_
