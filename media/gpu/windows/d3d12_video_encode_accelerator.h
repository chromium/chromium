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

#include "base/containers/queue.h"
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
        ID3D12VideoDevice3* video_device) = 0;
  };

  explicit D3D12VideoEncodeAccelerator(
      Microsoft::WRL::ComPtr<ID3D12Device> device);
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
  void Destroy() override;

  base::SingleThreadTaskRunner* GetEncoderTaskRunnerForTesting() const;
  size_t GetInputFramesQueueSizeForTesting() const;
  size_t GetBitstreamBuffersSizeForTesting() const;

 private:
  void InitializeTask(const Config& config);

  void UseOutputBitstreamBufferTask(BitstreamBuffer buffer);

  void RequestEncodingParametersChangeTask(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size);

  Microsoft::WRL::ComPtr<ID3D12Resource>
  CreateResourceForGpuMemoryBufferVideoFrame(const VideoFrame& frame);

  Microsoft::WRL::ComPtr<ID3D12Resource>
  CreateResourceForSharedMemoryVideoFrame(const VideoFrame& frame);

  void EncodeTask(scoped_refptr<VideoFrame> frame,
                  const VideoEncoder::EncodeOptions& options);

  void DoEncodeTask(scoped_refptr<VideoFrame> frame,
                    const VideoEncoder::EncodeOptions& options,
                    const BitstreamBuffer& bitstream_buffer);

  void DestroyTask();

  void NotifyError(EncoderStatus status);

  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device_;

  // Task runner for interacting with the client, and its checker.
  const scoped_refptr<base::SequencedTaskRunner> child_task_runner_;
  SEQUENCE_CHECKER(child_sequence_checker_);

  // Encoder sequence and its checker. All tasks are executed on it.
  const scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;
  SEQUENCE_CHECKER(encoder_sequence_checker_);

  VideoEncoderInfo encoder_info_;

  Config config_;
  size_t bitstream_buffer_size_ = 0;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;
  std::unique_ptr<MediaLog> media_log_;

  bool error_occurred_ = false;

  std::unique_ptr<D3D12CopyCommandQueueWrapper> copy_command_queue_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  std::unique_ptr<VideoEncodeDelegateFactoryInterface> encoder_factory_;
  std::unique_ptr<D3D12VideoEncodeDelegate> encoder_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  size_t num_frames_in_flight_ = 0;

  // Used for frame format conversion.
  VideoFrameConverter frame_converter_;

  struct InputFrameRef;

  base::queue<InputFrameRef> input_frames_queue_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  base::queue<BitstreamBuffer> bitstream_buffers_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

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
