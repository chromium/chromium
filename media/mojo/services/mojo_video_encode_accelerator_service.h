// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/lru_cache.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MojoMediaLog;

// This class implements the interface mojom::VideoEncodeAccelerator.
class MEDIA_MOJO_EXPORT MojoVideoEncodeAcceleratorService
    : public mojom::VideoEncodeAccelerator,
      public VideoEncodeAccelerator::Client {
 public:
  using GetCommandBufferHelperCB =
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>;
  // Create and initialize a VEA. Returns nullptr if either part fails.
  using CreateAndInitializeVideoEncodeAcceleratorCallback =
      base::OnceCallback<std::unique_ptr<::media::VideoEncodeAccelerator>(
          const ::media::VideoEncodeAccelerator::Config& config,
          Client* client,
          const gpu::GpuPreferences& gpu_preferences,
          const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
          const gpu::GPUInfo::GPUDevice& gpu_device,
          std::unique_ptr<MediaLog> media_log,
          GetCommandBufferHelperCB get_command_buffer_helper_cb,
          scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner)>;

  static void Create(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver,
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GPUInfo::GPUDevice& gpu_device,
      GetCommandBufferHelperCB get_command_buffer_helper_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner);

  MojoVideoEncodeAcceleratorService(
      CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GPUInfo::GPUDevice& gpu_device,
      GetCommandBufferHelperCB get_command_buffer_helper_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner);

  MojoVideoEncodeAcceleratorService(const MojoVideoEncodeAcceleratorService&) =
      delete;
  MojoVideoEncodeAcceleratorService& operator=(
      const MojoVideoEncodeAcceleratorService&) = delete;

  ~MojoVideoEncodeAcceleratorService() override;

  // mojom::VideoEncodeAccelerator impl.
  void Initialize(
      const media::VideoEncodeAccelerator::Config& config,
      mojo::PendingAssociatedRemote<mojom::VideoEncodeAcceleratorClient> client,
      mojo::PendingRemote<mojom::MediaLog> media_log,
      InitializeCallback callback) override;
  void Encode(const scoped_refptr<VideoFrame>& frame,
              const media::VideoEncoder::EncodeOptions& options,
              EncodeCallback callback) override;
  void UseOutputBitstreamBuffer(int32_t bitstream_buffer_id,
                                base::UnsafeSharedMemoryRegion region) override;
  void RequestEncodingParametersChangeWithBitrate(
      const media::Bitrate& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void RequestEncodingParametersChangeWithLayers(
      const media::VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void IsFlushSupported(IsFlushSupportedCallback callback) override;
  void Flush(FlushCallback callback) override;

 private:
  friend class MojoVideoEncodeAcceleratorIntegrationTest;
  friend class MojoVideoEncodeAcceleratorServiceTest;

  // VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyErrorStatus(const EncoderStatus& status) override;
  void NotifyEncoderInfoChange(const ::media::VideoEncoderInfo& info) override;

  CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback_;
  const gpu::GpuPreferences gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  const gpu::GPUInfo::GPUDevice gpu_device_;
  GetCommandBufferHelperCB get_command_buffer_helper_cb_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // Owned pointer to the underlying VideoEncodeAccelerator.
  std::unique_ptr<::media::VideoEncodeAccelerator> encoder_;
  mojo::AssociatedRemote<mojom::VideoEncodeAcceleratorClient> vea_client_;

  // Proxy object for providing media log services.
  std::unique_ptr<MojoMediaLog> media_log_;

  // Cache of parameters for sanity verification.
  size_t output_buffer_size_;
  gfx::Size input_coded_size_;
  bool supports_frame_size_change;

  base::LRUCache<int64_t, base::TimeTicks> timestamps_;

  // Note that this class is already thread hostile when bound.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MojoVideoEncodeAcceleratorService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODE_ACCELERATOR_SERVICE_H_
