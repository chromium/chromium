// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"

#include "third_party/skia/include/core/SkPixelRef.h"

namespace mojo {
namespace {

// Maximum reasonable width and height. We don't try to deserialize bitmaps
// bigger than these dimensions.
// These limits are fairly large to accommodate images from the largest possible
// canvas.
constexpr int kMaxWidth = 64 * 1024;
constexpr int kMaxHeight = 64 * 1024;

// A custom SkPixelRef subclass to wrap a BigBuffer storing the pixel data.
class BigBufferPixelRef final : public SkPixelRef {
 public:
  BigBufferPixelRef(mojo_base::BigBuffer buffer,
                    int width,
                    int height,
                    int row_bytes)
      : SkPixelRef(width, height, buffer.data(), row_bytes),
        buffer_(std::move(buffer)) {}
  ~BigBufferPixelRef() override = default;

 private:
  mojo_base::BigBuffer buffer_;
};

}  // namespace

// static
bool StructTraits<skia::mojom::BitmapDataView, SkBitmap>::IsNull(
    const SkBitmap& b) {
  return b.isNull();
}

// static
void StructTraits<skia::mojom::BitmapDataView, SkBitmap>::SetToNull(
    SkBitmap* b) {
  b->reset();
}

// static
const SkImageInfo& StructTraits<skia::mojom::BitmapDataView,
                                SkBitmap>::image_info(const SkBitmap& b) {
  return b.info();
}

// static
uint64_t StructTraits<skia::mojom::BitmapDataView, SkBitmap>::row_bytes(
    const SkBitmap& b) {
  return b.rowBytes();
}

// static
mojo_base::BigBufferView StructTraits<skia::mojom::BitmapDataView,
                                      SkBitmap>::pixel_data(const SkBitmap& b) {
  return mojo_base::BigBufferView(base::make_span(
      static_cast<uint8_t*>(b.getPixels()), b.computeByteSize()));
}

// static
bool StructTraits<skia::mojom::BitmapDataView, SkBitmap>::Read(
    skia::mojom::BitmapDataView data,
    SkBitmap* b) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  // Ensure width and height are reasonable.
  if (image_info.width() > kMaxWidth || image_info.height() > kMaxHeight)
    return false;

  *b = SkBitmap();
  if (!b->tryAllocPixels(image_info, data.row_bytes())) {
    return false;
  }

  // If the image is empty, return success after setting the image info.
  if (image_info.width() == 0 || image_info.height() == 0)
    return true;

  mojo_base::BigBufferView pixel_data_view;
  if (!data.ReadPixelData(&pixel_data_view))
    return false;

  base::span<const uint8_t> pixel_data_bytes = pixel_data_view.data();
  if (b->width() != image_info.width() || b->height() != image_info.height() ||
      static_cast<uint64_t>(b->rowBytes()) != data.row_bytes() ||
      b->computeByteSize() != pixel_data_bytes.size() || !b->readyToDraw()) {
    return false;
  }

  // Implementation note: This copy is important from a security perspective as
  // it provides the recipient of the SkBitmap with a stable copy of the data.
  // The sender could otherwise continue modifying the shared memory buffer
  // underlying the BigBuffer instance.
  std::copy(pixel_data_bytes.begin(), pixel_data_bytes.end(),
            static_cast<uint8_t*>(b->getPixels()));
  b->notifyPixelsChanged();
  return true;
}

// static
bool StructTraits<skia::mojom::UnsafeBitmapDataView, SkBitmap>::IsNull(
    const SkBitmap& b) {
  return b.isNull();
}

// static
void StructTraits<skia::mojom::UnsafeBitmapDataView, SkBitmap>::SetToNull(
    SkBitmap* b) {
  b->reset();
}

// static
const SkImageInfo& StructTraits<skia::mojom::UnsafeBitmapDataView,
                                SkBitmap>::image_info(const SkBitmap& b) {
  return b.info();
}

// static
uint64_t StructTraits<skia::mojom::UnsafeBitmapDataView, SkBitmap>::row_bytes(
    const SkBitmap& b) {
  return b.rowBytes();
}

// static
mojo_base::BigBufferView StructTraits<skia::mojom::UnsafeBitmapDataView,
                                      SkBitmap>::pixel_data(const SkBitmap& b) {
  return mojo_base::BigBufferView(base::make_span(
      static_cast<uint8_t*>(b.getPixels()), b.computeByteSize()));
}

// static
bool StructTraits<skia::mojom::UnsafeBitmapDataView, SkBitmap>::Read(
    skia::mojom::UnsafeBitmapDataView data,
    SkBitmap* b) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  // Ensure width and height are reasonable.
  if (image_info.width() > kMaxWidth || image_info.height() > kMaxHeight)
    return false;

  *b = SkBitmap();

  // If the image is empty, return success after setting the image info.
  if (image_info.width() == 0 || image_info.height() == 0)
    return b->tryAllocPixels(image_info, data.row_bytes());

  // Otherwise, set a custom PixelRef to retain the BigBuffer. This avoids
  // making another copy of the pixel data.

  mojo_base::BigBufferView pixel_data_view;
  if (!data.ReadPixelData(&pixel_data_view))
    return false;

  if (!b->setInfo(image_info, data.row_bytes()))
    return false;

  // Allow the resultant SkBitmap to refer to the given BigBuffer. Note, the
  // sender could continue modifying the pixels of the buffer, which could be a
  // security concern for some applications. The trade-off is performance.
  b->setPixelRef(
      sk_make_sp<BigBufferPixelRef>(
          mojo_base::BigBufferView::ToBigBuffer(std::move(pixel_data_view)),
          image_info.width(), image_info.height(), data.row_bytes()),
      0, 0);
  return true;
}

// static
bool StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap>::IsNull(
    const SkBitmap& b) {
  return b.isNull();
}

// static
void StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap>::SetToNull(
    SkBitmap* b) {
  b->reset();
}

// static
const SkImageInfo& StructTraits<skia::mojom::InlineBitmapDataView,
                                SkBitmap>::image_info(const SkBitmap& b) {
  return StructTraits<skia::mojom::BitmapDataView, SkBitmap>::image_info(b);
}

// static
uint64_t StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap>::row_bytes(
    const SkBitmap& b) {
  return StructTraits<skia::mojom::BitmapDataView, SkBitmap>::row_bytes(b);
}

// static
base::span<const uint8_t>
StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap>::pixel_data(
    const SkBitmap& b) {
  return base::make_span(static_cast<uint8_t*>(b.getPixels()),
                         b.computeByteSize());
}

// static
bool StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap>::Read(
    skia::mojom::InlineBitmapDataView data,
    SkBitmap* b) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  // Ensure width and height are reasonable.
  if (image_info.width() > kMaxWidth || image_info.height() > kMaxHeight)
    return false;

  *b = SkBitmap();
  if (!b->tryAllocPixels(image_info, data.row_bytes()))
    return false;

  // If the image is empty, return success after setting the image info.
  if (image_info.width() == 0 || image_info.height() == 0)
    return true;

  mojo::ArrayDataView<uint8_t> data_view;
  data.GetPixelDataDataView(&data_view);
  if (b->width() != image_info.width() || b->height() != image_info.height() ||
      static_cast<uint64_t>(b->rowBytes()) != data.row_bytes() ||
      b->computeByteSize() != data_view.size() || !b->readyToDraw()) {
    return false;
  }

  auto bitmap_buffer = base::make_span(static_cast<uint8_t*>(b->getPixels()),
                                       b->computeByteSize());
  if (!data.ReadPixelData(&bitmap_buffer) ||
      bitmap_buffer.size() != b->computeByteSize()) {
    return false;
  }

  b->notifyPixelsChanged();
  return true;
}

}  // namespace mojo
