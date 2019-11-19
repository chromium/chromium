// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

// Wrap ID3D11VideoProcessor to provide nicer methods for initialization,
// color space modification, and output/input view creation.
class MEDIA_GPU_EXPORT VideoProcessorProxy {
 public:
  VideoProcessorProxy(ComD3D11VideoDevice video_device,
                      ComD3D11DeviceContext d3d11_device_context);
  virtual ~VideoProcessorProxy();

  virtual bool Init(uint32_t width, uint32_t height);

  // TODO(tmathmeyer) implement color space modification.

  virtual HRESULT CreateVideoProcessorOutputView(
      ID3D11Texture2D* output_texture,
      D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* output_view_descriptor,
      ID3D11VideoProcessorOutputView** output_view);

  virtual HRESULT CreateVideoProcessorInputView(
      ID3D11Texture2D* input_texture,
      D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* input_view_descriptor,
      ID3D11VideoProcessorInputView** input_view);

  virtual HRESULT VideoProcessorBlt(ID3D11VideoProcessorOutputView* output_view,
                                    UINT output_frameno,
                                    UINT stream_count,
                                    D3D11_VIDEO_PROCESSOR_STREAM* streams);

 private:
  ComD3D11VideoDevice video_device_;
  ComD3D11VideoProcessorEnumerator processor_enumerator_;
  ComD3D11VideoProcessor video_processor_;
  ComD3D11DeviceContext device_context_;
  ComD3D11VideoContext video_context_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_
