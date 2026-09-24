// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_BACKEND_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_BACKEND_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/sequence_checker.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/d3d_video_decoder_backend.h"
#include "media/media_buildflags.h"

namespace media {

// D3D11 resource implementation of `D3DVideoDecoderBackend`. It may use a D3D11
// or D3D12 decode wrapper while retaining D3D11 resources and picture buffers.
class MEDIA_GPU_EXPORT D3D11VideoDecoderBackend
    : public D3DVideoDecoderBackend {
 public:
  D3D11VideoDecoderBackend();
  ~D3D11VideoDecoderBackend() override;

  // D3DVideoDecoderBackend:
  std::vector<SupportedVideoDecoderConfig> GetSupportedVideoDecoderConfigs(
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      GetD3DDeviceCB get_d3d_device_cb) const override;
  D3DStatus AcquireDeviceResources(GetD3DDeviceCB get_d3d_device_cb) override;
  void LogDecoderAdapterInfo(MediaLog* media_log) override;
  D3DStatus::Or<std::unique_ptr<D3DDecoderConfigurator>>
  CreateDecoderConfigurator(uint8_t bit_depth,
                            const VideoDecoderConfig& config,
                            VideoChromaSampling chroma_sampling,
                            const gpu::GpuPreferences& gpu_preferences,
                            const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
                            bool use_shared_handle,
                            MediaLog* media_log) override;
  D3DStatus::Or<std::unique_ptr<TextureSelector>> CreateTextureSelector(
      D3DDecoderConfigurator* decoder_configurator,
      const VideoDecoderConfig& config,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      bool use_shared_handle,
      MediaLog* media_log) override;
  D3DStatus::Or<std::unique_ptr<D3DVideoDecoderWrapper>>
  CreateVideoDecoderWrapper(GetD3DDeviceCB get_d3d_device_cb,
                            D3DDecoderConfigurator* decoder_configurator,
                            const VideoDecoderConfig& config,
                            uint8_t bit_depth,
                            VideoChromaSampling chroma_sampling,
                            int max_decode_requests,
                            MediaLog* media_log) override;
  std::unique_ptr<Texture2DWrapper> CreateOutputTextureWrapper(
      TextureSelector* texture_selector,
      const gfx::ColorSpace& color_space,
      const gfx::Size& size) override;
  D3DStatus::Or<scoped_refptr<D3DPictureBuffer>> CreateAndInitPictureBuffer(
      const gfx::Size& size,
      uint32_t array_size,
      size_t array_slice,
      size_t picture_index,
      bool use_single_video_decoder_texture,
      std::unique_ptr<Texture2DWrapper> texture_wrapper,
      D3DDecoderConfigurator* decoder_configurator,
      TextureSelector* texture_selector,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
          get_helper_cb,
      MediaLog* media_log,
      base::OnceCallback<void(scoped_refptr<D3DPictureBuffer>)> init_done_cb)
      override;
  D3DStatus WaitForDecodeComplete(D3DPictureBuffer* picture_buffer) override;
  bool ShouldUseDXVADeviceForHEVCRangeExtension(
      const VideoDecoderConfig& config) const override;
  VideoDecoderType GetDecoderType() const override;

 private:
  D3DStatus::Or<ComD3D11Texture2D> CreateDecoderOutputTexture(
      const gfx::Size& size,
      uint32_t array_size,
      D3DDecoderConfigurator* decoder_configurator,
      TextureSelector* texture_selector);

  SEQUENCE_CHECKER(sequence_checker_);

  // These may be accessed from the decoder task runner, since the ANGLE device
  // is in multi-threaded mode. Do not set global state on them.
  ComD3D11Device device_;
  ComD3D11DeviceContext device_context_;
  ComD3D11VideoDevice1 video_device_;

  ComD3D11Texture2D picture_buffer_texture_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_BACKEND_H_
