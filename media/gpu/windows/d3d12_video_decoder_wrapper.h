// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_DECODER_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_DECODER_WRAPPER_H_

#include <wrl.h>

#include <memory>

#include "media/base/video_decoder_config.h"
#include "media/base/video_types.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/d3d_video_decoder_wrapper.h"

namespace media {

// A wrapper class for API calls around ID3D12VideoDecoder.
class D3D12VideoDecoderWrapper : public D3DVideoDecoderWrapper {
 public:
  static std::unique_ptr<D3D12VideoDecoderWrapper> Create(
      MediaLog* media_log,
      ComD3D12VideoDevice video_device,
      VideoDecoderConfig config,
      uint8_t bit_depth,
      VideoChromaSampling chroma_sampling);

 protected:
  explicit D3D12VideoDecoderWrapper(MediaLog* media_log);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_DECODER_WRAPPER_H_
