// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_

#include <cstdint>

#include "base/memory/ref_counted.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "ui/gfx/color_space.h"

namespace media {

// Wrap ID3D11VideoProcessor to provide nicer methods for initialization,
// color space modification, and output/input view creation.
class MEDIA_GPU_EXPORT VideoProcessorProxy
    : public base::RefCounted<VideoProcessorProxy> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  VideoProcessorProxy(ComD3D11VideoDevice video_device,
                      ComD3D11DeviceContext d3d11_device_context);

  virtual D3D11Status Init(uint32_t width, uint32_t height);

  // TODO(tmathmeyer) implement color space modification.

  virtual HRESULT CreateVideoProcessorOutputView(
      ID3D11Texture2D* output_texture,
      D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* output_view_descriptor,
      ID3D11VideoProcessorOutputView** output_view);

  virtual HRESULT CreateVideoProcessorInputView(
      ID3D11Texture2D* input_texture,
      D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* input_view_descriptor,
      ID3D11VideoProcessorInputView** input_view);

  // Configure the stream (input) color space on the video context.
  virtual void SetStreamColorSpace(const gfx::ColorSpace& color_space);

  // Configure the output color space on the video context.
  virtual void SetOutputColorSpace(const gfx::ColorSpace& color_space);

  virtual HRESULT VideoProcessorBlt(ID3D11VideoProcessorOutputView* output_view,
                                    UINT output_frameno,
                                    UINT stream_count,
                                    D3D11_VIDEO_PROCESSOR_STREAM* streams);

 protected:
  virtual ~VideoProcessorProxy();
  friend class base::RefCounted<VideoProcessorProxy>;

 private:
  ComD3D11VideoDevice video_device_;
  ComD3D11VideoProcessorEnumerator processor_enumerator_;
  ComD3D11VideoProcessor video_processor_;
  ComD3D11DeviceContext device_context_;
  ComD3D11VideoContext video_context_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_
