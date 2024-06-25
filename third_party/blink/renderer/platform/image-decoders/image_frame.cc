/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

ImageFrame::ImageFrame() = default;

ImageFrame::~ImageFrame() = default;

ImageFrame::ImageFrame(const ImageFrame& other) : has_alpha_(false) {
  operator=(other);
}

ImageFrame& ImageFrame::operator=(const ImageFrame& other) {
  if (this == &other) {
    return *this;
  }

  bitmap_ = other.bitmap_;
  // Be sure to assign this before calling SetStatus(), since SetStatus() may
  // call NotifyBitmapIfPixelsChanged().
  pixels_changed_ = other.pixels_changed_;
  SetMemoryAllocator(other.GetAllocator());
  SetOriginalFrameRect(other.OriginalFrameRect());
  SetStatus(other.GetStatus());
  if (other.Timestamp()) {
    SetTimestamp(*other.Timestamp());
  } else {
    timestamp_.reset();
  }
  SetDuration(other.Duration());
  SetDisposalMethod(other.GetDisposalMethod());
  SetAlphaBlendSource(other.GetAlphaBlendSource());
  SetPremultiplyAlpha(other.PremultiplyAlpha());
  // Be sure that this is called after we've called SetStatus(), since we
  // look at our status to know what to do with the alpha value.
  SetHasAlpha(other.HasAlpha());
  pixel_format_ = other.pixel_format_;
  SetRequiredPreviousFrameIndex(other.RequiredPreviousFrameIndex());
  return *this;
}

void ImageFrame::ClearPixelData() {
  bitmap_.reset();
  status_ = kFrameEmpty;
  // NOTE: Do not reset other members here; ClearFrameBufferCache()
  // calls this to free the bitmap data, but other functions like
  // InitFrameBuffer() and FrameComplete() may still need to read
  // other metadata out of this frame later.
}

void ImageFrame::ZeroFillPixelData() {
  bitmap_.eraseARGB(0, 0, 0, 0);
  has_alpha_ = true;
}

bool ImageFrame::CopyBitmapData(const ImageFrame& other) {
  DCHECK_NE(this, &other);
  has_alpha_ = other.has_alpha_;
  pixel_format_ = other.pixel_format_;
  bitmap_.reset();
  SkImageInfo info = other.bitmap_.info();
  if (!bitmap_.tryAllocPixels(info)) {
    return false;
  }

  if (!other.bitmap_.readPixels(info, bitmap_.getPixels(), bitmap_.rowBytes(),
                                0, 0)) {
    return false;
  }

  status_ = kFrameInitialized;
  return true;
}

bool ImageFrame::TakeBitmapDataIfWritable(ImageFrame* other) {
  DCHECK(other);
  DCHECK_EQ(kFrameComplete, other->status_);
  DCHECK_EQ(kFrameEmpty, status_);
  DCHECK_NE(this, other);
  if (other->bitmap_.isImmutable()) {
    return false;
  }
  has_alpha_ = other->has_alpha_;
  pixel_format_ = other->pixel_format_;
  bitmap_.reset();
  bitmap_.swap(other->bitmap_);
  other->status_ = kFrameEmpty;
  status_ = kFrameInitialized;
  return true;
}

bool ImageFrame::AllocatePixelData(int new_width,
                                   int new_height,
                                   sk_sp<SkColorSpace> color_space) {
  // AllocatePixelData() should only be called once.
  DCHECK(!Width() && !Height());
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  if (new_width > 1000 || new_height > 1000) {
    return false;
  }
#endif

  SkImageInfo info = SkImageInfo::MakeN32(
      new_width, new_height,
      premultiply_alpha_ ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      std::move(color_space));
  if (pixel_format_ == kRGBA_F16) {
    info = info.makeColorType(kRGBA_F16_SkColorType);
  }
  bool success = bitmap_.setInfo(info);
  DCHECK(success);
  success = bitmap_.tryAllocPixels(allocator_);
  if (success) {
    status_ = kFrameInitialized;
  }

  return success;
}

sk_sp<SkImage> ImageFrame::FinalizePixelsAndGetImage() {
  DCHECK_EQ(kFrameComplete, status_);
  bitmap_.setImmutable();
  return SkImages::RasterFromBitmap(bitmap_);
}

void ImageFrame::SetHasAlpha(bool alpha) {
  has_alpha_ = alpha;

  bitmap_.setAlphaType(ComputeAlphaType());
}

void ImageFrame::SetStatus(Status status) {
  status_ = status;
  if (status_ == kFrameComplete) {
    bitmap_.setAlphaType(ComputeAlphaType());
    // Send pending pixels changed notifications now, because we can't do
    // this after the bitmap has been marked immutable.  We don't set the
    // bitmap immutable here because it would defeat
    // TakeBitmapDataIfWritable().  Instead we let the bitmap stay mutable
    // until someone calls FinalizePixelsAndGetImage() to actually get the
    // SkImage.
    NotifyBitmapIfPixelsChanged();
  }
}

void ImageFrame::ZeroFillFrameRect(const gfx::Rect& rect) {
  if (rect.IsEmpty()) {
    return;
  }

  bitmap_.eraseArea(gfx::RectToSkIRect(rect), SkColorSetARGB(0, 0, 0, 0));
  SetHasAlpha(true);
}

static void BlendRGBAF16Buffer(ImageFrame::PixelDataF16* dst,
                               ImageFrame::PixelDataF16* src,
                               size_t num_pixels,
                               SkAlphaType dst_alpha_type) {
  // Source is always unpremul, but the blending result might be premul or
  // unpremul, depending on the alpha type of the destination pixel passed to
  // this function.
  SkImageInfo info = SkImageInfo::Make(base::checked_cast<int>(num_pixels), 1,
                                       kRGBA_F16_SkColorType, dst_alpha_type,
                                       SkColorSpace::MakeSRGBLinear());
  sk_sp<SkSurface> surface =
      SkSurfaces::WrapPixels(info, dst, info.minRowBytes());

  SkPixmap src_pixmap(info.makeAlphaType(kUnpremul_SkAlphaType), src,
                      info.minRowBytes());
  sk_sp<SkImage> src_image =
      SkImages::RasterFromPixmap(src_pixmap, nullptr, nullptr);

  surface->getCanvas()->drawImage(src_image, 0, 0);
}

void ImageFrame::BlendRGBARawF16Buffer(PixelDataF16* dst,
                                       PixelDataF16* src,
                                       size_t num_pixels) {
  BlendRGBAF16Buffer(dst, src, num_pixels, kUnpremul_SkAlphaType);
}

void ImageFrame::BlendRGBAPremultipliedF16Buffer(PixelDataF16* dst,
                                                 PixelDataF16* src,
                                                 size_t num_pixels) {
  BlendRGBAF16Buffer(dst, src, num_pixels, kPremul_SkAlphaType);
}

static uint8_t BlendChannel(uint8_t src,
                            uint8_t src_a,
                            uint8_t dst,
                            uint8_t dst_a,
                            unsigned scale) {
  unsigned blend_unscaled = src * src_a + dst * dst_a;
  DCHECK(blend_unscaled < (1ULL << 32) / scale);
  return (blend_unscaled * scale) >> 24;
}

static uint32_t BlendSrcOverDstNonPremultiplied(uint32_t src, uint32_t dst) {
  uint8_t src_a = SkGetPackedA32(src);
  if (src_a == 0) {
    return dst;
  }

  uint8_t dst_a = SkGetPackedA32(dst);
  uint8_t dst_factor_a = (dst_a * SkAlpha255To256(255 - src_a)) >> 8;
  DCHECK(src_a + dst_factor_a < (1U << 8));
  uint8_t blend_a = src_a + dst_factor_a;
  unsigned scale = (1UL << 24) / blend_a;

  uint8_t blend_r = BlendChannel(SkGetPackedR32(src), src_a,
                                 SkGetPackedR32(dst), dst_factor_a, scale);
  uint8_t blend_g = BlendChannel(SkGetPackedG32(src), src_a,
                                 SkGetPackedG32(dst), dst_factor_a, scale);
  uint8_t blend_b = BlendChannel(SkGetPackedB32(src), src_a,
                                 SkGetPackedB32(dst), dst_factor_a, scale);

  return SkPackARGB32(blend_a, blend_r, blend_g, blend_b);
}

void ImageFrame::BlendRGBARaw(PixelData* dest,
                              unsigned r,
                              unsigned g,
                              unsigned b,
                              unsigned a) {
  *dest = BlendSrcOverDstNonPremultiplied(SkPackARGB32(a, r, g, b), *dest);
}

void ImageFrame::BlendSrcOverDstRaw(PixelData* src, PixelData dst) {
  *src = BlendSrcOverDstNonPremultiplied(*src, dst);
}

SkAlphaType ImageFrame::ComputeAlphaType() const {
  // If the frame is not fully loaded, there will be transparent pixels,
  // so we can't tell skia we're opaque, even for image types that logically
  // always are (e.g. jpeg).
  if (!has_alpha_ && status_ == kFrameComplete) {
    return kOpaque_SkAlphaType;
  }

  return premultiply_alpha_ ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
}

}  // namespace blink
