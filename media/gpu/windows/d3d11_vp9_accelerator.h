// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <windows.h>
#include <wrl/client.h>

#include "media/base/media_log.h"
#include "media/gpu/vp9_decoder.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {
class CdmProxyContext;

class D3D11VP9Accelerator : public VP9Decoder::VP9Accelerator {
 public:
  D3D11VP9Accelerator(
      D3D11VideoDecoderClient* client,
      MediaLog* media_log,
      CdmProxyContext* cdm_proxy_context,
      Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder,
      Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
      Microsoft::WRL::ComPtr<ID3D11VideoContext1> video_context);
  ~D3D11VP9Accelerator() override;

  scoped_refptr<VP9Picture> CreateVP9Picture() override;

  // TODO(crbug/890054): Use constref instead of scoped_refptr.
  bool SubmitDecode(
      const scoped_refptr<VP9Picture>& picture,
      const Vp9SegmentationParams& segmentation_params,
      const Vp9LoopFilterParams& loop_filter_params,
      const std::vector<scoped_refptr<VP9Picture>>& reference_pictures,
      const base::Closure& on_finished_cb) override;

  bool OutputPicture(const scoped_refptr<VP9Picture>& picture) override;

  bool IsFrameContextRequired() const override;

  bool GetFrameContext(const scoped_refptr<VP9Picture>& picture,
                       Vp9FrameContext* frame_context) override;

 private:
  // Helper methods for SubmitDecode
  bool BeginFrame(D3D11VP9Picture* pic);

  // TODO(crbug/890054): Use constref instead of scoped_refptr.
  void CopyFrameParams(const scoped_refptr<D3D11VP9Picture>& pic,
                       DXVA_PicParams_VP9* pic_params);
  void CopyReferenceFrames(
      const scoped_refptr<D3D11VP9Picture>& pic,
      DXVA_PicParams_VP9* pic_params,
      const std::vector<scoped_refptr<VP9Picture>>& reference_pictures);
  void CopyFrameRefs(DXVA_PicParams_VP9* pic_params,
                     const scoped_refptr<D3D11VP9Picture>& picture);
  void CopyLoopFilterParams(DXVA_PicParams_VP9* pic_params,
                            const Vp9LoopFilterParams& loop_filter_params);
  void CopyQuantParams(DXVA_PicParams_VP9* pic_params,
                       const scoped_refptr<D3D11VP9Picture>& pic);
  void CopySegmentationParams(DXVA_PicParams_VP9* pic_params,
                              const Vp9SegmentationParams& segmentation_params);
  void CopyHeaderSizeAndID(DXVA_PicParams_VP9* pic_params,
                           const scoped_refptr<D3D11VP9Picture>& pic);
  bool SubmitDecoderBuffer(const DXVA_PicParams_VP9& pic_params,
                           const scoped_refptr<D3D11VP9Picture>& pic);

  void RecordFailure(const std::string& fail_type, const std::string& reason);

  D3D11VideoDecoderClient* client_;
  MediaLog* const media_log_;
  CdmProxyContext* cdm_proxy_context_;
  UINT status_feedback_;
  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext1> video_context_;

  DISALLOW_COPY_AND_ASSIGN(D3D11VP9Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_
