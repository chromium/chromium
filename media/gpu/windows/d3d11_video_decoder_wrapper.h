// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_WRAPPER_H_

#include <memory>

#include "media/base/video_decoder_config.h"
#include "media/gpu/windows/d3d11_decoder_configurator.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/d3d_video_decoder_wrapper.h"

namespace media {

// A wrapper class for API calls around ID3D11VideoDecoder.
class D3D11VideoDecoderWrapper : public D3DVideoDecoderWrapper {
 public:
  static std::unique_ptr<D3D11VideoDecoderWrapper> Create(
      MediaLog* media_log,
      ComD3D11VideoDevice video_device,
      ComD3D11VideoContext video_context,
      const D3D11DecoderConfigurator* decoder_configurator,
      D3D_FEATURE_LEVEL supported_d3d11_version,
      VideoDecoderConfig config);
  ~D3D11VideoDecoderWrapper() override;

 protected:
  explicit D3D11VideoDecoderWrapper(MediaLog* media_log);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_WRAPPER_H_
