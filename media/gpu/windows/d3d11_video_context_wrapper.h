// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_CONTEXT_WRAPPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_CONTEXT_WRAPPER_H_

#include <d3d11_1.h>
#include <wrl/client.h>
#include <memory>

#include "base/macros.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

class MEDIA_GPU_EXPORT VideoContextWrapper {
 public:
  VideoContextWrapper() = default;
  virtual ~VideoContextWrapper();
  // D3D11_VIDEO_DECODER_BUFFER_DESC1 and D3D11_VIDEO_DECODER_BUFFER_DESC
  // have radically different sets of member variables, which means that in
  // order to have a converter class, we need to support all fields. They are
  // also in different orders, which prevents using a simple memcpy.
  // It seems we don't currently use any of the D3D11_VIDEO_DECODER_BUFFER_DESC
  // specific fields right now.
  // Fields are named as they appear in the D3D11 Structs.
  struct VideoBufferWrapper {
    // Shared Fields
    D3D11_VIDEO_DECODER_BUFFER_TYPE BufferType;
    UINT DataOffset;
    UINT DataSize;
    void* pIV;
    UINT IVSize;

    // DESC1-specific fields
    D3D11_VIDEO_DECODER_SUB_SAMPLE_MAPPING_BLOCK* pSubSampleMappingBlock;
    UINT SubSampleMappingCount;
  };

  static std::unique_ptr<VideoContextWrapper> CreateWrapper(
      D3D_FEATURE_LEVEL supported_d3d11_version,
      ComD3D11DeviceContext device_context,
      HRESULT* status);

  // This method signiture is defined to match exactly that of
  // |ID3D11VideoContext::DecoderBeginFrame|.
  virtual HRESULT DecoderBeginFrame(ID3D11VideoDecoder* video_decoder,
                                    ID3D11VideoDecoderOutputView* output_view,
                                    UINT content_key_size,
                                    const void* content_key) = 0;

  // This method signiture is defined to match exactly that of
  // |ID3D11VideoContext::GetDecoderBuffer|.
  virtual HRESULT GetDecoderBuffer(ID3D11VideoDecoder* video_decoder,
                                   D3D11_VIDEO_DECODER_BUFFER_TYPE type,
                                   UINT* buffer_size,
                                   void** buffer) = 0;

  // This method signiture is defined to match exactly that of
  // |ID3D11VideoContext::ReleaseDecoderBuffer|.
  virtual HRESULT ReleaseDecoderBuffer(
      ID3D11VideoDecoder* video_decoder,
      D3D11_VIDEO_DECODER_BUFFER_TYPE type) = 0;

  // This method signiture is defined to match exactly that of
  // |ID3D11VideoContext::DecoderEndFrame|.
  virtual HRESULT DecoderEndFrame(ID3D11VideoDecoder* video_decoder) = 0;

  // This method signiture is defined to match exactly that of
  // |ID3D11VideoContext::SubmitDecoderBuffers|.
  virtual HRESULT SubmitDecoderBuffers(ID3D11VideoDecoder* video_decoder,
                                       UINT num_buffers,
                                       const VideoBufferWrapper* buffers) = 0;

  DISALLOW_COPY_AND_ASSIGN(VideoContextWrapper);
};  // VideoContextWrapper

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_CONTEXT_WRAPPER_H_
