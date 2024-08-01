// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/fake_libva_driver/av1_decoder_delegate.h"

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_surface.h"
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media::internal {
namespace {

// std::unique_ptr release helpers. We need to release both the containing
// structs as well as refs held within the structures.
struct ScopedDav1dDataFree {
  void operator()(void* x) const {
    auto* data = static_cast<Dav1dData*>(x);
    dav1d_data_unref(data);
    delete data;
  }
};

struct ScopedDav1dPictureFree {
  void operator()(void* x) const {
    auto* pic = static_cast<Dav1dPicture*>(x);
    dav1d_picture_unref(pic);
    delete pic;
  }
};

void NullFreeCallback(const uint8_t* buffer, void* opaque) {}

}  // namespace

void Av1DecoderDelegate::Dav1dContextDeleter::operator()(Dav1dContext* ptr) {
  dav1d_close(&ptr);
}

Av1DecoderDelegate::Av1DecoderDelegate(VAProfile profile) {
  CHECK_EQ(profile, VAProfileAV1Profile0);

  Dav1dSettings settings;
  dav1d_default_settings(&settings);
  settings.max_frame_delay = 1;

  Dav1dContext* temp_david_context_pointer = nullptr;
  CHECK_EQ(dav1d_open(&temp_david_context_pointer, &settings), 0);

  dav1d_context_ = std::unique_ptr<Dav1dContext, Dav1dContextDeleter>(
      temp_david_context_pointer);
}

Av1DecoderDelegate::~Av1DecoderDelegate() {
  dav1d_context_.reset();
}

void Av1DecoderDelegate::SetRenderTarget(const FakeSurface& surface) {
  render_target_ = &surface;
}

void Av1DecoderDelegate::EnqueueWork(
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

void Av1DecoderDelegate::Run() {
  CHECK(render_target_);
  CHECK(encoded_data_buffer_);

  using ScopedPtrDav1dData = std::unique_ptr<Dav1dData, ScopedDav1dDataFree>;
  ScopedPtrDav1dData input_buffer(new Dav1dData{0});

  // TODO(bchoobineh): Currently this wrap logic does not account for
  // superframes which would require specifying a data offset and updated
  // data size. Will need to find/compute the data offset/size from data
  // passed by Chrome to support these bitstreams.
  CHECK_EQ(
      dav1d_data_wrap(input_buffer.get(),
                      static_cast<uint8_t*>(encoded_data_buffer_->GetData()),
                      encoded_data_buffer_->GetDataSize(),
                      /*free_callback=*/&NullFreeCallback,
                      /*cookie=*/nullptr),
      0);

  CHECK_EQ(dav1d_send_data(dav1d_context_.get(), input_buffer.get()), 0);

  CHECK_EQ(input_buffer->sz, 0u);

  using ScopedPtrDav1dPicture =
      std::unique_ptr<Dav1dPicture, ScopedDav1dPictureFree>;
  ScopedPtrDav1dPicture av1_pic(new Dav1dPicture{0});
  CHECK_EQ(dav1d_get_picture(dav1d_context_.get(), av1_pic.get()), 0);

  // We currently only support reading from I420 and into NV12.
  CHECK_EQ(av1_pic->p.layout, DAV1D_PIXEL_LAYOUT_I420);
  CHECK_EQ(render_target_->GetVAFourCC(),
           static_cast<uint32_t>(VA_FOURCC_NV12));

  const ScopedBOMapping& bo_mapping = render_target_->GetMappedBO();
  CHECK(bo_mapping.IsValid());
  const ScopedBOMapping::ScopedAccess mapped_bo = bo_mapping.BeginAccess();

  const int convert_result = libyuv::I420ToNV12(
      /*src_y=*/static_cast<uint8_t*>(av1_pic->data[0]),
      /*src_stride_y=*/base::checked_cast<int>(av1_pic->stride[0]),
      /*src_u=*/static_cast<uint8_t*>(av1_pic->data[1]),
      /*src_stride_u=*/base::checked_cast<int>(av1_pic->stride[1]),
      /*src_v=*/static_cast<uint8_t*>(av1_pic->data[2]),
      /*src_stride_v=*/base::checked_cast<int>(av1_pic->stride[1]),
      /*dst_y=*/mapped_bo.GetData(0),
      /*dst_stride_y=*/base::checked_cast<int>(mapped_bo.GetStride(0)),
      /*dst_uv=*/mapped_bo.GetData(1),
      /*dst_stride_uv=*/base::checked_cast<int>(mapped_bo.GetStride(1)),
      /*width=*/av1_pic->p.w,
      /*height=*/av1_pic->p.h);
  CHECK_EQ(convert_result, 0);

  encoded_data_buffer_ = nullptr;
}

}  // namespace media::internal
