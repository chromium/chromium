// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/fake_libva_driver/fake_image.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_driver.h"

namespace media::internal {

std::unique_ptr<FakeImage> FakeImage::Create(IdType id,
                                             const VAImageFormat& format,
                                             int width,
                                             int height,
                                             FakeDriver& fake_driver,
                                             VAImage* va_image) {
  // Chrome should only request NV12 images from the fake driver.
  CHECK_EQ(format.fourcc, static_cast<uint32_t>(VA_FOURCC_NV12));

  // Validate the |format|. Chrome should request VA_LSB_FIRST images only.
  CHECK_EQ(format.byte_order, static_cast<uint32_t>(VA_LSB_FIRST));
  CHECK_EQ(format.bits_per_pixel, 12u);

  std::vector<Plane> planes;
  planes.emplace_back(/*stride=*/base::checked_cast<uint32_t>(width),
                      /*offset=*/0);

  // UV stride = ceil(width / 2) * 2.
  base::CheckedNumeric<uint32_t> uv_stride(base::checked_cast<uint32_t>(width));
  uv_stride += 1;
  uv_stride /= 2;
  uv_stride *= 2;

  // UV offset = Y plane size = width * height.
  base::CheckedNumeric<uint32_t> uv_offset(base::checked_cast<uint32_t>(width));
  uv_offset *= base::checked_cast<uint32_t>(height);

  planes.emplace_back(/*stride=*/uv_stride.ValueOrDie(),
                      /*offset=*/uv_offset.ValueOrDie());

  // UV plane size = ceil(height / 2) * UV stride.
  base::CheckedNumeric<uint32_t> uv_size(base::checked_cast<uint32_t>(height));
  uv_size += 1;
  uv_size /= 2;
  uv_size *= uv_stride;

  // Total size = UV offset + UV plane size.
  base::CheckedNumeric<unsigned int> data_size(
      uv_offset.ValueOrDie<unsigned int>());
  data_size += uv_size.ValueOrDie<unsigned int>();

  memset(va_image, 0, sizeof(VAImage));
  va_image->image_id = id;
  va_image->format = format;

  FakeBuffer::IdType buf = fake_driver.CreateBuffer(
      /*context=*/VA_INVALID_ID, VAImageBufferType,
      /*size_per_element=*/1, data_size.ValueOrDie(), /*data=*/nullptr);
  va_image->buf = buf;

  va_image->width = base::checked_cast<uint16_t>(width);
  va_image->height = base::checked_cast<uint16_t>(height);
  va_image->data_size = data_size.ValueOrDie<uint32_t>();
  va_image->num_planes = 2;
  va_image->pitches[0] = base::checked_cast<uint32_t>(width);
  va_image->pitches[1] = uv_stride.ValueOrDie<uint32_t>();
  va_image->offsets[0] = 0;
  va_image->offsets[1] = uv_offset.ValueOrDie<uint32_t>();

  return base::WrapUnique(
      new FakeImage(id, format, width, height, std::move(planes),
                    fake_driver.GetBuffer(buf), fake_driver));
}

FakeImage::FakeImage(FakeImage::IdType id,
                     const VAImageFormat& format,
                     int width,
                     int height,
                     std::vector<Plane> planes,
                     const FakeBuffer& buffer,
                     FakeDriver& driver)
    : id_(id),
      format_(format),
      width_(width),
      height_(height),
      planes_(std::move(planes)),
      buffer_(buffer),
      driver_(driver) {}

FakeImage::~FakeImage() {
  driver_->DestroyBuffer(buffer_->GetID());
}

FakeImage::IdType FakeImage::GetID() const {
  return id_;
}

const VAImageFormat& FakeImage::GetFormat() const {
  return format_;
}

int FakeImage::GetWidth() const {
  return width_;
}

int FakeImage::GetHeight() const {
  return height_;
}

const FakeBuffer& FakeImage::GetBuffer() const {
  return *buffer_;
}

uint32_t FakeImage::GetPlaneStride(size_t plane) const {
  CHECK_LT(plane, planes_.size());
  return planes_[plane].stride;
}

uint32_t FakeImage::GetPlaneOffset(size_t plane) const {
  CHECK_LT(plane, planes_.size());
  return planes_[plane].offset;
}

FakeImage::Plane::Plane(uint32_t stride, uint32_t offset)
    : stride(stride), offset(offset) {}

}  // namespace media::internal
