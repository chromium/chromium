// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_ACCELERATOR_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12video.h"
// Windows SDK headers should be included after DirectX headers.

#include <wrl.h>

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/lru_cache.h"
#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_copy_command_list_wrapper.h"
#include "media/gpu/windows/d3d12_video_encode_delegate.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class CommandBufferHelper;
class VEAEncodingLatencyMetricsHelper;

typedef base::OnceCallback<void(scoped_refptr<VideoFrame> frame,
                                base::win::ScopedHandle shared_handle,
                                HRESULT hr)>
    FrameAvailableCB;

class MEDIA_GPU_EXPORT D3D12VideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  class VideoEncodeDelegateFactoryInterface {
   public:
    virtual ~VideoEncodeDelegateFactoryInterface() = default;
    virtual std::unique_ptr<D3D12VideoEncodeDelegate> CreateVideoEncodeDelegate(
        ID3D12VideoDevice3* video_device,
        VideoCodecProfile profile) = 0;
    virtual SupportedProfiles GetSupportedProfiles(
        ID3D12VideoDevice3* video_device,
        const std::vector<D3D12_VIDEO_ENCODER_CODEC>& codecs) = 0;
  };

  using GetCommandBufferStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*()>;
  explicit D3D12VideoEncodeAccelerator(
      Microsoft::WRL::ComPtr<ID3D12Device> device,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);
  ~D3D12VideoEncodeAccelerator() override;

  void SetEncoderFactoryForTesting(
      std::unique_ptr<VideoEncodeDelegateFactoryInterface> encoder_factory);

  SupportedProfiles GetSupportedProfiles() override;
  EncoderStatus Initialize(const Config& config,
                           Client* client,
                           std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              const VideoEncoder::EncodeOptions& options) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;
  void SetCommandBufferHelperCB(
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
          command_buffer_helper_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) override;

  struct GetCommandBufferHelperResult {
    GetCommandBufferHelperResult();
    GetCommandBufferHelperResult(const GetCommandBufferHelperResult& other);
    ~GetCommandBufferHelperResult();
    scoped_refptr<CommandBufferHelper> command_buffer_helper;
    Microsoft::WRL::ComPtr<ID3D11Device> shared_d3d_device;
  };

  base::SingleThreadTaskRunner* GetEncoderTaskRunnerForTesting() const;
  size_t GetInputFramesQueueSizeForTesting() const;
  size_t GetBitstreamBuffersSizeForTesting() const;
  size_t GetSharedHandleCacheSizeForTesting() const;

 private:
  struct InputFrameRef;

  void InitializeTask(const Config& config, const SupportedProfiles& profile);

  void UseOutputBitstreamBufferTask(BitstreamBuffer buffer);

  void RequestEncodingParametersChangeTask(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size);

  Microsoft::WRL::ComPtr<ID3D12Resource>
  CreateResourceForGpuMemoryBufferVideoFrame(const VideoFrame& frame);

  Microsoft::WRL::ComPtr<ID3D12Resource>
  CreateResourceForSharedMemoryVideoFrame(const VideoFrame& frame);

  void EncodeTask(scoped_refptr<VideoFrame> frame,
                  const VideoEncoder::EncodeOptions& options);

  void DoEncodeTask(const InputFrameRef& input_frame,
                    const BitstreamBuffer& bitstream_buffer);

  void TryEncodeFrames();

  void ResolveQueuedSharedImages();

  void DestroyTask();

  void FlushTask();

  void NotifyFlushDone(bool succeed);

  void NotifyError(EncoderStatus status);

  // Invoked when the CommandBufferHelper is available.
  void OnCommandBufferHelperAvailable(
      const GetCommandBufferHelperResult& result);

  // Invoked when a shared image backed VideoFrame is resolved.
  void OnSharedImageResolved(scoped_refptr<VideoFrame> frame,
                             base::win::ScopedHandle shared_handle,
                             HRESULT hr);

  std::vector<D3D12_VIDEO_ENCODER_CODEC> codecs_;

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device_;

  // Task runner for interacting with the client, and its checker.
  const scoped_refptr<base::SequencedTaskRunner> child_task_runner_;
  SEQUENCE_CHECKER(child_sequence_checker_);

  // Encoder sequence and its checker. All tasks are executed on it.
  const scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;
  SEQUENCE_CHECKER(encoder_sequence_checker_);

  // Used to post tasks to the gpu thread for shared image access
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // Helper for accessing shared textures.
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;

  VideoEncoderInfo encoder_info_;

  std::unique_ptr<VEAEncodingLatencyMetricsHelper> metrics_helper_;
  bool encoded_at_least_one_frame_ = false;

  Config config_;
  size_t bitstream_buffer_size_ = 0;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;
  std::unique_ptr<MediaLog> media_log_;

  bool error_occurred_ = false;

  // True if Destroy() has been called.
  bool destroy_requested_ GUARDED_BY_CONTEXT(child_sequence_checker_) = false;

  // True if a flush request is pending.
  bool flush_requested_ GUARDED_BY_CONTEXT(encoder_sequence_checker_) = false;

  // The accelerator has acquired the command buffer helper that
  // would be used for accessing incoming shared images.
  bool acquired_command_buffer_ = false;

  std::unique_ptr<D3D12CopyCommandQueueWrapper> copy_command_queue_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  std::unique_ptr<VideoEncodeDelegateFactoryInterface> encoder_factory_;
  std::unique_ptr<D3D12VideoEncodeDelegate> encoder_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  size_t num_frames_in_flight_ = 0;

  // Used for frame format conversion.
  VideoFrameConverter frame_converter_;

  // Invoked once flush is completed.
  FlushCallback flush_callback_;

  base::circular_deque<InputFrameRef> input_frames_queue_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  base::queue<BitstreamBuffer> bitstream_buffers_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Cache for shared handle to D3D12Resource mapping when caching is enabled.
  // LRU cache that maps DXGIHandleToken to the corresponding ID3D12Resource.
  base::LRUCache<gfx::DXGIHandleToken, Microsoft::WRL::ComPtr<ID3D12Resource>>
      shared_handle_cache_ GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // WeakPtr of this, bound to |child_task_runner_|.
  base::WeakPtr<D3D12VideoEncodeAccelerator> child_weak_this_;
  // WeakPtr of this, bound to |encoder_task_runner_|.
  base::WeakPtr<D3D12VideoEncodeAccelerator> encoder_weak_this_;
  base::WeakPtrFactory<D3D12VideoEncodeAccelerator> child_weak_this_factory_{
      this};
  base::WeakPtrFactory<D3D12VideoEncodeAccelerator> encoder_weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_ENCODE_ACCELERATOR_H_
