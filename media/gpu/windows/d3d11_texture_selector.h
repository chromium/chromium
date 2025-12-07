// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_TEXTURE_SELECTOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_TEXTURE_SELECTOR_H_

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MediaLog;
class FormatSupportChecker;

// Stores different pixel formats and DGXI formats, and checks for decoder
// GUID support.
class MEDIA_GPU_EXPORT TextureSelector {
 public:
  TextureSelector(VideoPixelFormat pixfmt,
                  DXGI_FORMAT output_dxgifmt,
                  ComD3D11VideoDevice video_device,
                  ComD3D11DeviceContext d3d11_device_context,
                  bool use_shared_handle);
  virtual ~TextureSelector();

  static std::unique_ptr<TextureSelector> Create(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      DXGI_FORMAT decoder_output_format,
      const FormatSupportChecker* format_checker,
      ComD3D11VideoDevice video_device,
      ComD3D11DeviceContext device_context,
      MediaLog* media_log,
      gfx::ColorSpace input_color_space,
      bool shared_image_use_shared_handle = false);

  virtual std::unique_ptr<Texture2DWrapper> CreateTextureWrapper(
      ComD3D11Device device,
      gfx::ColorSpace color_space,
      gfx::Size size);

  virtual bool DoesDecoderOutputUseSharedHandle() const;

  VideoPixelFormat PixelFormat() const { return pixel_format_; }
  DXGI_FORMAT OutputDXGIFormat() const { return output_dxgifmt_; }
  bool DoesSharedImageUseSharedHandle() const {
    return shared_image_use_shared_handle_;
  }

  virtual bool WillCopyForTesting() const;

 protected:
  const ComD3D11VideoDevice& video_device() const { return video_device_; }

  const ComD3D11DeviceContext& device_context() const {
    return device_context_;
  }

 private:
  friend class CopyTextureSelector;

  const VideoPixelFormat pixel_format_;
  const DXGI_FORMAT output_dxgifmt_;

  ComD3D11VideoDevice video_device_;
  ComD3D11DeviceContext device_context_;

  bool shared_image_use_shared_handle_;
};

class MEDIA_GPU_EXPORT CopyTextureSelector : public TextureSelector {
 public:
  CopyTextureSelector(VideoPixelFormat pixfmt,
                      DXGI_FORMAT output_dxgifmt,
                      ComD3D11VideoDevice video_device,
                      ComD3D11DeviceContext d3d11_device_context,
                      bool use_shared_handle);
  ~CopyTextureSelector() override;

  std::unique_ptr<Texture2DWrapper> CreateTextureWrapper(
      ComD3D11Device device,
      gfx::ColorSpace color_space,
      gfx::Size size) override;

  bool DoesDecoderOutputUseSharedHandle() const override;

  bool WillCopyForTesting() const override;

 private:
  scoped_refptr<VideoProcessorProxy> video_processor_proxy_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_TEXTURE_SELECTOR_H_
