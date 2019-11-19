// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_context_wrapper.h"

#include <memory>

#include "base/memory/weak_ptr.h"

namespace media {

VideoContextWrapper::~VideoContextWrapper() = default;

// Specialization for sample submission
template <typename CTX>
struct BufferSubmitter;

template <>
struct BufferSubmitter<ID3D11VideoContext1> {
  static HRESULT SubmitDecoderBuffers(
      ComD3D11VideoContext1 context,
      ID3D11VideoDecoder* decoder,
      const UINT num_buffers,
      const VideoContextWrapper::VideoBufferWrapper* src) {
    constexpr UINT max_buffers = 4;
    DCHECK_LE(num_buffers, max_buffers);
    D3D11_VIDEO_DECODER_BUFFER_DESC1 buffers[max_buffers] = {};
    memset(buffers, 0, sizeof(D3D11_VIDEO_DECODER_BUFFER_DESC1) * max_buffers);
    for (size_t i = 0; i < num_buffers; i++) {
      buffers[i].BufferType = src[i].BufferType;
      buffers[i].DataOffset = src[i].DataOffset;
      buffers[i].DataSize = src[i].DataSize;
      buffers[i].pIV = src[i].pIV;
      buffers[i].IVSize = src[i].IVSize;
      buffers[i].pSubSampleMappingBlock = src[i].pSubSampleMappingBlock;
      buffers[i].SubSampleMappingCount = src[i].SubSampleMappingCount;
    }
    return context->SubmitDecoderBuffers1(decoder, num_buffers, buffers);
  }
};

template <>
struct BufferSubmitter<ID3D11VideoContext> {
  static HRESULT SubmitDecoderBuffers(
      ComD3D11VideoContext context,
      ID3D11VideoDecoder* decoder,
      const UINT num_buffers,
      const VideoContextWrapper::VideoBufferWrapper* src) {
    constexpr UINT max_buffers = 4;
    DCHECK_LE(num_buffers, max_buffers);
    D3D11_VIDEO_DECODER_BUFFER_DESC buffers[max_buffers] = {};
    memset(buffers, 0, sizeof(D3D11_VIDEO_DECODER_BUFFER_DESC) * max_buffers);
    for (size_t i = 0; i < num_buffers; i++) {
      buffers[i].BufferType = src[i].BufferType;
      buffers[i].DataOffset = src[i].DataOffset;
      buffers[i].DataSize = src[i].DataSize;
      DCHECK_EQ(src[i].pIV, nullptr);
      DCHECK_EQ(src[i].IVSize, 0u);
      DCHECK_EQ(src[i].pSubSampleMappingBlock, nullptr);
      DCHECK_EQ(src[i].SubSampleMappingCount, 0u);
    }
    return context->SubmitDecoderBuffers(decoder, num_buffers, buffers);
  }
};

// CTX is The type of D3D11VideoContext* that we are wrapping, whether that
// be D3D11VideoContext or D3D11VideoContext1. Only these types will work
// since the BufferSubmitter struct is only specialized for these two types.
template <typename CTX>
class VideoContextWrapperImpl : public VideoContextWrapper {
 public:
  explicit VideoContextWrapperImpl(Microsoft::WRL::ComPtr<CTX> video_context)
      : video_context_(video_context) {}
  ~VideoContextWrapperImpl() {}

  HRESULT DecoderBeginFrame(ID3D11VideoDecoder* video_decoder,
                            ID3D11VideoDecoderOutputView* output_view,
                            UINT content_key_size,
                            const void* content_key) override {
    return video_context_->DecoderBeginFrame(video_decoder, output_view,
                                             content_key_size, content_key);
  }

  HRESULT GetDecoderBuffer(ID3D11VideoDecoder* video_decoder,
                           D3D11_VIDEO_DECODER_BUFFER_TYPE type,
                           UINT* buffer_size,
                           void** buffer) override {
    return video_context_->GetDecoderBuffer(video_decoder, type, buffer_size,
                                            buffer);
  }

  HRESULT ReleaseDecoderBuffer(ID3D11VideoDecoder* video_decoder,
                               D3D11_VIDEO_DECODER_BUFFER_TYPE type) override {
    return video_context_->ReleaseDecoderBuffer(video_decoder, type);
  }

  HRESULT DecoderEndFrame(ID3D11VideoDecoder* video_decoder) override {
    return video_context_->DecoderEndFrame(video_decoder);
  }

  HRESULT SubmitDecoderBuffers(ID3D11VideoDecoder* video_decoder,
                               UINT num_buffers,
                               const VideoBufferWrapper* buffers) override {
    return BufferSubmitter<CTX>::SubmitDecoderBuffers(
        video_context_, video_decoder, num_buffers, buffers);
  }

 private:
  Microsoft::WRL::ComPtr<CTX> video_context_;
};

std::unique_ptr<VideoContextWrapper> VideoContextWrapper::CreateWrapper(
    D3D_FEATURE_LEVEL supported_d3d11_version,
    ComD3D11DeviceContext device_context,
    HRESULT* status) {
  if (supported_d3d11_version == D3D_FEATURE_LEVEL_11_0) {
    ComD3D11VideoContext video_context;
    *status = device_context.CopyTo(video_context.ReleaseAndGetAddressOf());
    return std::make_unique<VideoContextWrapperImpl<ID3D11VideoContext>>(
        video_context);
  }

  if (supported_d3d11_version == D3D_FEATURE_LEVEL_11_1) {
    ComD3D11VideoContext1 video_context;
    *status = device_context.CopyTo(video_context.ReleaseAndGetAddressOf());
    return std::make_unique<VideoContextWrapperImpl<ID3D11VideoContext1>>(
        video_context);
  }

  *status = E_FAIL;
  return nullptr;
}

}  // namespace media
