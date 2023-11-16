// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_VPX_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_VPX_DECODER_DELEGATE_H_

#include "media/gpu/vaapi/test/fake_libva_driver/context_delegate.h"

#include <va/va.h>

struct vpx_codec_ctx;

namespace media::internal {

// Class used for libvpx software decoding.
class VpxDecoderDelegate : public ContextDelegate {
 public:
  VpxDecoderDelegate(int picture_width_hint,
                     int picture_height_hint,
                     VAProfile profile);
  VpxDecoderDelegate(const VpxDecoderDelegate&) = delete;
  VpxDecoderDelegate& operator=(const VpxDecoderDelegate&) = delete;
  ~VpxDecoderDelegate() override;

  // ContextDelegate implementation.
  void SetRenderTarget(const FakeSurface& surface) override;
  void EnqueueWork(
      const std::vector<raw_ptr<const FakeBuffer>>& buffers) override;
  void Run() override;

 private:
  raw_ptr<const FakeBuffer> encoded_data_buffer_{nullptr};
  raw_ptr<const FakeSurface> render_target_{nullptr};
  std::unique_ptr<vpx_codec_ctx> vpx_codec_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_VPX_DECODER_DELEGATE_H_
