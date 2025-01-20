// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_H264_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_H264_DECODER_DELEGATE_H_

#include <va/va.h>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "media/gpu/vaapi/test/fake_libva_driver/context_delegate.h"
#include "third_party/openh264/src/codec/api/wels/codec_api.h"

namespace media::internal {

// Class used for h264 software decoding.
class H264DecoderDelegate : public ContextDelegate {
 public:
  H264DecoderDelegate(int picture_width_hint,
                      int picture_height_hint,
                      VAProfile profile);
  H264DecoderDelegate(const H264DecoderDelegate&) = delete;
  H264DecoderDelegate& operator=(const H264DecoderDelegate&) = delete;
  ~H264DecoderDelegate() override;

  // ContextDelegate implementation.
  void SetRenderTarget(const FakeSurface& surface) override;
  void EnqueueWork(
      const std::vector<raw_ptr<const FakeBuffer>>& buffers) override;
  void Run() override;

 private:
  void OnFrameReady(unsigned char* pData[3], SBufferInfo* pDstInfo);

  raw_ptr<ISVCDecoder> svc_decoder_ = nullptr;
  const VAProfile profile_;

  std::vector<raw_ptr<const FakeBuffer>> slice_data_buffers_;
  std::vector<raw_ptr<const FakeBuffer>> slice_param_buffers_;

  raw_ptr<const FakeSurface> render_target_{nullptr};
  raw_ptr<const FakeBuffer> pic_param_buffer_{nullptr};
  raw_ptr<const FakeBuffer> matrix_buffer_{nullptr};

  uint32_t current_ts_ = 0;
  base::LRUCache<uint32_t, raw_ptr<const FakeSurface>> ts_to_render_target_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_H264_DECODER_DELEGATE_H_
