// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"

#include "base/ranges/algorithm.h"
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

bool CreateSkBitmapForPixelData(SkBitmap* b,
                                const SkImageInfo& image_info,
                                base::span<const uint8_t> pixel_data) {
  // Ensure width and height are reasonable.
  if (image_info.width() > kMaxWidth || image_info.height() > kMaxHeight)
    return false;

  // We require incoming bitmaps to be tightly packed by specifying the
  // rowBytes() as minRowBytes(). Then we compare the number of bytes against
  // `pixel_data.size()` later to verify the actual data is tightly packed.
  if (!b->tryAllocPixels(image_info, image_info.minRowBytes()))
    return false;

  // If the image is empty, return success after setting the image info.
  if (image_info.width() == 0 || image_info.height() == 0)
    return true;

  // If these don't match then the number of bytes sent does not match what the
  // rest of the mojom said there should be.
  if (pixel_data.size() != b->computeByteSize())
    return false;

  // Implementation note: This copy is important from a security perspective as
  // it provides the recipient of the SkBitmap with a stable copy of the data.
  // The sender could otherwise continue modifying the shared memory buffer
  // underlying the BigBuffer instance.
  base::ranges::copy(pixel_data, static_cast<uint8_t*>(b->getPixels()));
  b->notifyPixelsChanged();
  return true;
}

}  // namespace

// static
mojo_base::BigBufferView StructTraits<skia::mojom::BitmapN32DataView,
                                      SkBitmap>::pixel_data(const SkBitmap& b) {
  CHECK_EQ(b.rowBytes(), b.info().minRowBytes());
  return mojo_base::BigBufferView(base::make_span(
      static_cast<uint8_t*>(b.getPixels()), b.computeByteSize()));
}

// static
bool StructTraits<skia::mojom::BitmapN32DataView, SkBitmap>::Read(
    skia::mojom::BitmapN32DataView data,
    SkBitmap* b) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  mojo_base::BigBufferView pixel_data_view;
  if (!data.ReadPixelData(&pixel_data_view))
    return false;

  return CreateSkBitmapForPixelData(b, std::move(image_info),
                                    pixel_data_view.data());
}

// static
mojo_base::BigBufferView
StructTraits<skia::mojom::BitmapWithArbitraryBppDataView, SkBitmap>::pixel_data(
    const SkBitmap& b) {
  CHECK_EQ(b.rowBytes(), b.info().minRowBytes());
  return mojo_base::BigBufferView(base::make_span(
      static_cast<uint8_t*>(b.getPixels()), b.computeByteSize()));
}

// static
bool StructTraits<skia::mojom::BitmapWithArbitraryBppDataView, SkBitmap>::Read(
    skia::mojom::BitmapWithArbitraryBppDataView data,
    SkBitmap* b) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  mojo_base::BigBufferView pixel_data_view;
  if (!data.ReadPixelData(&pixel_data_view))
    return false;

  return CreateSkBitmapForPixelData(b, std::move(image_info),
                                    pixel_data_view.data());
}

// static
mojo_base::BigBufferView
StructTraits<skia::mojom::BitmapMappedFromTrustedProcessDataView,
             SkBitmap>::pixel_data(const SkBitmap& b) {
  CHECK_EQ(b.rowBytes(), b.info().minRowBytes());
  return mojo_base::BigBufferView(base::make_span(
      static_cast<uint8_t*>(b.getPixels()), b.computeByteSize()));
}

// static
bool StructTraits<
    skia::mojom::BitmapMappedFromTrustedProcessDataView,
    SkBitmap>::Read(skia::mojom::BitmapMappedFromTrustedProcessDataView data,
                    SkBitmap* b) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;

  // Ensure width and height are reasonable.
  if (image_info.width() > kMaxWidth || image_info.height() > kMaxHeight)
    return false;

  // If the image is empty, return success after setting the image info.
  if (image_info.width() == 0 || image_info.height() == 0)
    return b->tryAllocPixels(image_info);

  // Otherwise, set a custom PixelRef to retain the BigBuffer. This avoids
  // making another copy of the pixel data.

  mojo_base::BigBufferView pixel_data_view;
  if (!data.ReadPixelData(&pixel_data_view))
    return false;

  // We require incoming bitmaps to be tightly packed by specifying the
  // rowBytes() as minRowBytes(). Then we compare the number of bytes against
  // `pixel_data_view.data().size()` later to verify the actual data is tightly
  // packed.
  if (!b->setInfo(image_info, image_info.minRowBytes()))
    return false;

  // If these don't match then the number of bytes sent does not match what the
  // rest of the mojom said there should be.
  if (b->computeByteSize() != pixel_data_view.data().size())
    return false;

  // Allow the resultant SkBitmap to refer to the given BigBuffer. Note, the
  // sender could continue modifying the pixels of the buffer, which could be a
  // security concern for some applications. The trade-off is performance.
  b->setPixelRef(
      sk_make_sp<BigBufferPixelRef>(
          mojo_base::BigBufferView::ToBigBuffer(std::move(pixel_data_view)),
          image_info.width(), image_info.height(), image_info.minRowBytes()),
      0, 0);
  return true;
}

// static
base::span<const uint8_t>
StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap>::pixel_data(
    const SkBitmap& b) {
  CHECK_EQ(b.rowBytes(), b.info().minRowBytes());
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

  mojo::ArrayDataView<uint8_t> pixel_data_view;
  data.GetPixelDataDataView(&pixel_data_view);

  base::span<const uint8_t> pixel_data_bytes(pixel_data_view.data(),
                                             pixel_data_view.size());

  return CreateSkBitmapForPixelData(b, std::move(image_info),
                                    std::move(pixel_data_bytes));
}

}  // namespace mojo
