/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"

namespace blink {

ImageDataBuffer::ImageDataBuffer(scoped_refptr<StaticBitmapImage> image) {
  if (!image)
    return;
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!paint_image || paint_image.IsPaintWorklet())
    return;

  SkImageInfo paint_image_info = paint_image.GetSkImageInfo();
  if (paint_image_info.isEmpty())
    return;

#if defined(MEMORY_SANITIZER)
  // Test if software SKImage has an initialized pixmap.
  SkPixmap pixmap;
  if (!paint_image.IsTextureBacked() &&
      paint_image.GetSwSkImage()->peekPixels(&pixmap)) {
    MSAN_CHECK_MEM_IS_INITIALIZED(pixmap.addr(), pixmap.computeByteSize());
  }
#endif

  if (paint_image.IsTextureBacked() || paint_image.IsLazyGenerated() ||
      paint_image_info.alphaType() != kUnpremul_SkAlphaType) {
    // Unpremul is handled upfront, using readPixels, which will correctly clamp
    // premul color values that would otherwise cause overflows in the skia
    // encoder unpremul logic.
    SkColorType colorType = paint_image.GetColorType();
    if (colorType == kRGBA_8888_SkColorType ||
        colorType == kBGRA_8888_SkColorType)
      colorType = kN32_SkColorType;  // Work around for bug with JPEG encoder
    const SkImageInfo info =
        SkImageInfo::Make(paint_image_info.width(), paint_image_info.height(),
                          paint_image_info.colorType(), kUnpremul_SkAlphaType,
                          paint_image_info.refColorSpace());
    const size_t rowBytes = info.minRowBytes();
    size_t size = info.computeByteSize(rowBytes);
    if (SkImageInfo::ByteSizeOverflowed(size))
      return;

    sk_sp<SkData> data = SkData::MakeUninitialized(size);
    pixmap_ = {info, data->writable_data(), info.minRowBytes()};
    if (!paint_image.readPixels(info, pixmap_.writable_addr(), rowBytes, 0,
                                0)) {
      pixmap_.reset();
      return;
    }
    MSAN_CHECK_MEM_IS_INITIALIZED(pixmap_.addr(), pixmap_.computeByteSize());
    retained_image_ = SkImages::RasterFromData(info, std::move(data), rowBytes);
  } else {
    retained_image_ = paint_image.GetSwSkImage();
    if (!retained_image_->peekPixels(&pixmap_))
      return;
    MSAN_CHECK_MEM_IS_INITIALIZED(pixmap_.addr(), pixmap_.computeByteSize());
  }
  is_valid_ = true;
  size_ = gfx::Size(image->width(), image->height());
}

ImageDataBuffer::ImageDataBuffer(const SkPixmap& pixmap)
    : pixmap_(pixmap), size_(gfx::Size(pixmap.width(), pixmap.height())) {
  is_valid_ = pixmap_.addr() && !size_.IsEmpty();
}

std::unique_ptr<ImageDataBuffer> ImageDataBuffer::Create(
    scoped_refptr<StaticBitmapImage> image) {
  std::unique_ptr<ImageDataBuffer> buffer =
      base::WrapUnique(new ImageDataBuffer(image));
  if (!buffer->IsValid())
    return nullptr;
  return buffer;
}

std::unique_ptr<ImageDataBuffer> ImageDataBuffer::Create(
    const SkPixmap& pixmap) {
  std::unique_ptr<ImageDataBuffer> buffer =
      base::WrapUnique(new ImageDataBuffer(pixmap));
  if (!buffer->IsValid())
    return nullptr;
  return buffer;
}

const unsigned char* ImageDataBuffer::Pixels() const {
  DCHECK(is_valid_);
  return static_cast<const unsigned char*>(pixmap_.addr());
}

bool ImageDataBuffer::EncodeImage(const ImageEncodingMimeType mime_type,
                                  const double& quality,
                                  Vector<unsigned char>* encoded_image) const {
  return EncodeImageInternal(mime_type, quality, encoded_image, pixmap_);
}

bool ImageDataBuffer::EncodeImageInternal(const ImageEncodingMimeType mime_type,
                                          const double& quality,
                                          Vector<unsigned char>* encoded_image,
                                          const SkPixmap& pixmap) const {
  DCHECK(is_valid_);

  if (mime_type == kMimeTypeJpeg) {
    SkJpegEncoder::Options options;
    options.fQuality = ImageEncoder::ComputeJpegQuality(quality);
    options.fAlphaOption = SkJpegEncoder::AlphaOption::kBlendOnBlack;
    if (options.fQuality == 100) {
      options.fDownsample = SkJpegEncoder::Downsample::k444;
    }
    return ImageEncoder::Encode(encoded_image, pixmap, options);
  }

  if (mime_type == kMimeTypeWebp) {
    SkWebpEncoder::Options options = ImageEncoder::ComputeWebpOptions(quality);
    return ImageEncoder::Encode(encoded_image, pixmap, options);
  }

  DCHECK_EQ(mime_type, kMimeTypePng);
  SkPngEncoder::Options options;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
  options.fZLibLevel = 3;
  return ImageEncoder::Encode(encoded_image, pixmap, options);
}

String ImageDataBuffer::ToDataURL(const ImageEncodingMimeType mime_type,
                                  const double& quality) const {
  DCHECK(is_valid_);
  Vector<unsigned char> result;
  if (!EncodeImageInternal(mime_type, quality, &result, pixmap_))
    return "data:,";

  return "data:" + ImageEncodingMimeTypeName(mime_type) + ";base64," +
         Base64Encode(result);
}

}  // namespace blink
