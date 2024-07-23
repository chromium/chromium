// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <dxva.h>

#include "base/memory/raw_ptr.h"
#include "media/base/media_log.h"
#include "media/gpu/vp9_decoder.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {

class D3D11VP9Accelerator : public VP9Decoder::VP9Accelerator {
 public:
  D3D11VP9Accelerator(D3D11VideoDecoderClient* client, MediaLog* media_log);

  D3D11VP9Accelerator(const D3D11VP9Accelerator&) = delete;
  D3D11VP9Accelerator& operator=(const D3D11VP9Accelerator&) = delete;

  ~D3D11VP9Accelerator() override;

  scoped_refptr<VP9Picture> CreateVP9Picture() override;

  Status SubmitDecode(scoped_refptr<VP9Picture> picture,
                      const Vp9SegmentationParams& segmentation_params,
                      const Vp9LoopFilterParams& loop_filter_params,
                      const Vp9ReferenceFrameVector& reference_frames) override;

  bool OutputPicture(scoped_refptr<VP9Picture> picture) override;

  bool NeedsCompressedHeaderParsed() const override;

 private:
  // Helper methods for SubmitDecode
  bool BeginFrame(const D3D11VP9Picture& pic);

  // TODO(crbug.com/40595783): Use constref instead of scoped_refptr.
  void CopyFrameParams(const D3D11VP9Picture& pic,
                       DXVA_PicParams_VP9* pic_params);
  void CopyReferenceFrames(const D3D11VP9Picture& pic,
                           DXVA_PicParams_VP9* pic_params,
                           const Vp9ReferenceFrameVector& ref_frames);
  void CopyFrameRefs(DXVA_PicParams_VP9* pic_params,
                     const D3D11VP9Picture& picture);
  void CopyLoopFilterParams(DXVA_PicParams_VP9* pic_params,
                            const Vp9LoopFilterParams& loop_filter_params);
  void CopyQuantParams(DXVA_PicParams_VP9* pic_params,
                       const D3D11VP9Picture& pic);
  void CopySegmentationParams(DXVA_PicParams_VP9* pic_params,
                              const Vp9SegmentationParams& segmentation_params);
  void CopyHeaderSizeAndID(DXVA_PicParams_VP9* pic_params,
                           const D3D11VP9Picture& pic);
  bool SubmitDecoderBuffer(const DXVA_PicParams_VP9& pic_params,
                           const D3D11VP9Picture& pic);

  std::unique_ptr<MediaLog> media_log_;
  raw_ptr<D3D11VideoDecoderClient> client_;

  UINT status_feedback_;

  // Used to set |use_prev_in_find_mv_refs| properly.
  gfx::Size last_frame_size_;
  bool last_show_frame_ = false;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_
