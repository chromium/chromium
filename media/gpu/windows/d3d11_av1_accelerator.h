// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_AV1_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_AV1_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_log.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"

typedef struct _DXVA_PicParams_AV1 DXVA_PicParams_AV1;
typedef struct _DXVA_Tile_AV1 DXVA_Tile_AV1;

namespace media {

class D3D11AV1Accelerator : public AV1Decoder::AV1Accelerator {
 public:
  D3D11AV1Accelerator(D3D11VideoDecoderClient* client,
                      MediaLog* media_log,
                      ComD3D11VideoDevice video_device,
                      std::unique_ptr<VideoContextWrapper> video_context);

  D3D11AV1Accelerator(const D3D11AV1Accelerator&) = delete;
  D3D11AV1Accelerator& operator=(const D3D11AV1Accelerator&) = delete;

  ~D3D11AV1Accelerator() override;

  scoped_refptr<AV1Picture> CreateAV1Picture(bool apply_grain) override;

  Status SubmitDecode(const AV1Picture& pic,
                      const libgav1::ObuSequenceHeader& seq_header,
                      const AV1ReferenceFrameVector& ref_frames,
                      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
                      base::span<const uint8_t> data) override;

  bool OutputPicture(const AV1Picture& pic) override;

 private:
  class ScopedDecoderBuffer;
  ScopedDecoderBuffer GetBuffer(D3D11_VIDEO_DECODER_BUFFER_TYPE type);

  bool SubmitDecoderBuffer(
      const DXVA_PicParams_AV1& pic_params,
      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers);

  void RecordFailure(const std::string& fail_type, D3D11Status error);
  void RecordFailure(const std::string& fail_type,
                     const std::string& message,
                     D3D11Status::Codes reason);
  void SetVideoDecoder(ComD3D11VideoDecoder video_decoder);
  void FillPicParams(size_t picture_index,
                     bool apply_grain,
                     const libgav1::ObuFrameHeader& frame_header,
                     const libgav1::ObuSequenceHeader& seq_header,
                     const AV1ReferenceFrameVector& ref_frames,
                     DXVA_PicParams_AV1* pp);

  raw_ptr<D3D11VideoDecoderClient> client_;
  std::unique_ptr<MediaLog> media_log_;
  ComD3D11VideoDecoder video_decoder_;
  ComD3D11VideoDevice video_device_;
  std::unique_ptr<VideoContextWrapper> video_context_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_AV1_ACCELERATOR_H_
