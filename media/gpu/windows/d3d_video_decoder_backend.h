// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D_VIDEO_DECODER_BACKEND_H_
#define MEDIA_GPU_WINDOWS_D3D_VIDEO_DECODER_BACKEND_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/d3d_status.h"

namespace gfx {
class ColorSpace;
class Size;
}  // namespace gfx

namespace gpu {
class GpuDriverBugWorkarounds;
struct GpuPreferences;
}  // namespace gpu

namespace media {

class CommandBufferHelper;
class D3DDecoderConfigurator;
class D3DPictureBuffer;
class D3DVideoDecoderWrapper;
class MediaLog;
class Texture2DWrapper;
class TextureSelector;
class VideoDecoderConfig;

enum class D3DVersion { kD3D11, kD3D12 };

using GetD3DDeviceCB = base::RepeatingCallback<ComUnknown(D3DVersion)>;

// Manages decoder resource state and wrapper/resource creation.
class D3DVideoDecoderBackend {
 public:
  D3DVideoDecoderBackend(const D3DVideoDecoderBackend&) = delete;
  D3DVideoDecoderBackend& operator=(const D3DVideoDecoderBackend&) = delete;
  virtual ~D3DVideoDecoderBackend() = default;

  virtual std::vector<SupportedVideoDecoderConfig>
  GetSupportedVideoDecoderConfigs(
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      GetD3DDeviceCB get_d3d_device_cb) const = 0;

  virtual D3DStatus AcquireDeviceResources(
      GetD3DDeviceCB get_d3d_device_cb) = 0;

  virtual void LogDecoderAdapterInfo(MediaLog* media_log) = 0;

  virtual D3DStatus::Or<std::unique_ptr<D3DDecoderConfigurator>>
  CreateDecoderConfigurator(uint8_t bit_depth,
                            const VideoDecoderConfig& config,
                            VideoChromaSampling chroma_sampling,
                            const gpu::GpuPreferences& gpu_preferences,
                            const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
                            bool use_shared_handle,
                            MediaLog* media_log) = 0;

  virtual D3DStatus::Or<std::unique_ptr<TextureSelector>> CreateTextureSelector(
      D3DDecoderConfigurator* decoder_configurator,
      const VideoDecoderConfig& config,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      bool use_shared_handle,
      MediaLog* media_log) = 0;

  virtual D3DStatus::Or<std::unique_ptr<D3DVideoDecoderWrapper>>
  CreateVideoDecoderWrapper(GetD3DDeviceCB get_d3d_device_cb,
                            D3DDecoderConfigurator* decoder_configurator,
                            const VideoDecoderConfig& config,
                            uint8_t bit_depth,
                            VideoChromaSampling chroma_sampling,
                            int max_decode_requests,
                            MediaLog* media_log) = 0;

  virtual std::unique_ptr<Texture2DWrapper> CreateOutputTextureWrapper(
      TextureSelector* texture_selector,
      const gfx::ColorSpace& color_space,
      const gfx::Size& size) = 0;

  virtual D3DStatus::Or<scoped_refptr<D3DPictureBuffer>>
  CreateAndInitPictureBuffer(
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
      base::OnceCallback<void(scoped_refptr<D3DPictureBuffer>)>
          init_done_cb) = 0;

  virtual D3DStatus WaitForDecodeComplete(D3DPictureBuffer* picture_buffer) = 0;

  // Returns true if standard DXVA HEVC range-extension profiles should be used
  // for `config`.
  virtual bool ShouldUseDXVADeviceForHEVCRangeExtension(
      const VideoDecoderConfig& config) const = 0;

  virtual VideoDecoderType GetDecoderType() const = 0;

 protected:
  D3DVideoDecoderBackend() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D_VIDEO_DECODER_BACKEND_H_
