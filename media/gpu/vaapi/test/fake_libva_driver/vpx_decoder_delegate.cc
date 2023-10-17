// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/vpx_decoder_delegate.h"

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_image.h"

namespace media::internal {

VpxDecoderDelegate::VpxDecoderDelegate(int picture_width_hint,
                                       int picture_height_hint) {
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = base::checked_cast<unsigned int>(picture_width_hint);
  vpx_config.h = base::checked_cast<unsigned int>(picture_height_hint);

  if (vpx_config.w >= 3840) {
    vpx_config.threads = 16;
  } else if (vpx_config.w >= 2560) {
    vpx_config.threads = 8;
  } else {
    vpx_config.threads = 4;
  }

  vpx_codec_ = std::make_unique<vpx_codec_ctx>();
  const vpx_codec_err_t status = vpx_codec_dec_init(
      vpx_codec_.get(), vpx_codec_vp9_dx(), &vpx_config, /*flags=*/0);

  CHECK_EQ(status, VPX_CODEC_OK);
}

VpxDecoderDelegate::~VpxDecoderDelegate() {
  vpx_codec_destroy(vpx_codec_.get());
}

void VpxDecoderDelegate::SetRenderTarget(const FakeSurface& surface) {
  render_target_ = &surface;
}

void VpxDecoderDelegate::EnqueueWork(
    const std::vector<raw_ptr<const FakeBuffer>>& buffers) {
  CHECK(render_target_);
  CHECK(!encoded_data_buffer_);
  for (auto buffer : buffers) {
    if (buffer->GetType() == VASliceDataBufferType) {
      encoded_data_buffer_ = buffer;
      return;
    }
  }
}

void VpxDecoderDelegate::Run() {
  CHECK(render_target_);
  CHECK(encoded_data_buffer_);
  const vpx_codec_err_t status = vpx_codec_decode(
      vpx_codec_.get(), static_cast<uint8_t*>(encoded_data_buffer_->GetData()),
      base::checked_cast<unsigned int>(encoded_data_buffer_->GetDataSize()),
      /*user_priv=*/nullptr,
      /*deadline=*/0);
  CHECK_EQ(status, VPX_CODEC_OK);

  vpx_codec_iter_t iter = nullptr;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(vpx_codec_.get(), &iter);
  CHECK(vpx_image);
}

}  // namespace media::internal
