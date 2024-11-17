// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_AV1_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_AV1_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>

#include "base/functional/callback_helpers.h"
#include "media/base/media_log.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"

typedef struct _DXVA_PicParams_AV1 DXVA_PicParams_AV1;
typedef struct _DXVA_Tile_AV1 DXVA_Tile_AV1;

namespace media {

class D3D11AV1Accelerator : public AV1Decoder::AV1Accelerator {
 public:
  D3D11AV1Accelerator(D3D11VideoDecoderClient* client,
                      MediaLog* media_log,
                      bool disable_invalid_ref);

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
  bool SubmitDecoderBuffer(
      const DXVA_PicParams_AV1& pic_params,
      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers);

  void FillPicParams(size_t picture_index,
                     bool apply_grain,
                     const libgav1::ObuFrameHeader& frame_header,
                     const libgav1::ObuSequenceHeader& seq_header,
                     const AV1ReferenceFrameVector& ref_frames,
                     DXVA_PicParams_AV1* pp);

  std::unique_ptr<MediaLog> media_log_;
  raw_ptr<D3D11VideoDecoderClient> client_;
  // When set to true, the accelerator will use current frame for the missing
  // reference.
  bool disable_invalid_ref_ = false;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_AV1_ACCELERATOR_H_
