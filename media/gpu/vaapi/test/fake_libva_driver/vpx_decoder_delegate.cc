// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/fake_libva_driver/vpx_decoder_delegate.h"

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_surface.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_image.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media::internal {

VpxDecoderDelegate::VpxDecoderDelegate(int picture_width_hint,
                                       int picture_height_hint,
                                       VAProfile profile) {
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
  CHECK(profile == VAProfileVP8Version0_3 || profile == VAProfileVP9Profile0);
  const vpx_codec_err_t status = vpx_codec_dec_init(
      vpx_codec_.get(),
      (profile == VAProfileVP8Version0_3) ? vpx_codec_vp8_dx()
                                          : vpx_codec_vp9_dx(),
      &vpx_config, /*flags=*/0);

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
  // The user of libva should ensure that at most one frame is available for
  // output for each vaBeginPicture()/vaRenderPicture()/vaEndPicture() cycle.
  CHECK(!vpx_codec_get_frame(vpx_codec_.get(), &iter));

  encoded_data_buffer_ = nullptr;

  // No show reference only frame.
  if (!vpx_image) {
    return;
  }

  // We currently only support reading from I420 and into NV12.
  CHECK_EQ(vpx_image->fmt, VPX_IMG_FMT_I420);
  CHECK_EQ(render_target_->GetVAFourCC(),
           static_cast<uint32_t>(VA_FOURCC_NV12));

  const ScopedBOMapping& bo_mapping = render_target_->GetMappedBO();
  CHECK(bo_mapping.IsValid());
  const ScopedBOMapping::ScopedAccess mapped_bo = bo_mapping.BeginAccess();

  const int convert_result = libyuv::I420ToNV12(
      /*src_y=*/vpx_image->planes[VPX_PLANE_Y],
      /*src_stride_y=*/vpx_image->stride[VPX_PLANE_Y],
      /*src_u=*/vpx_image->planes[VPX_PLANE_U],
      /*src_stride_u=*/vpx_image->stride[VPX_PLANE_U],
      /*src_v=*/vpx_image->planes[VPX_PLANE_V],
      /*src_stride_v=*/vpx_image->stride[VPX_PLANE_V],
      /*dst_y=*/mapped_bo.GetData(0),
      /*dst_stride_y=*/base::checked_cast<int>(mapped_bo.GetStride(0)),
      /*dst_uv=*/mapped_bo.GetData(1),
      /*dst_stride_uv=*/base::checked_cast<int>(mapped_bo.GetStride(1)),
      /*width=*/base::checked_cast<int>(vpx_image->d_w),
      /*height=*/base::checked_cast<int>(vpx_image->d_h));
  CHECK_EQ(convert_result, 0);
}

}  // namespace media::internal
