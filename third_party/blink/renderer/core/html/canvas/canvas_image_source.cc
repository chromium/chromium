// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {
namespace {

std::unique_ptr<CanvasResourceProvider> CreateProvider(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    const SkImageInfo& info,
    const scoped_refptr<StaticBitmapImage>& source_image,
    bool fallback_to_software) {
  const cc::PaintFlags::FilterQuality filter_quality =
      cc::PaintFlags::FilterQuality::kLow;
  if (context_provider) {
    constexpr bool kIsOriginTopLeft = true;
    const uint32_t usage_flags =
        context_provider->ContextProvider()
            ->SharedImageInterface()
            ->UsageForMailbox(source_image->GetMailboxHolder().mailbox);
    auto resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
        info, filter_quality, CanvasResourceProvider::ShouldInitialize::kNo,
        context_provider, RasterMode::kGPU, kIsOriginTopLeft, usage_flags);
    if (resource_provider)
      return resource_provider;

    if (!fallback_to_software)
      return nullptr;
  }

  return CanvasResourceProvider::CreateBitmapProvider(
      info, filter_quality, CanvasResourceProvider::ShouldInitialize::kNo);
}

}  // anonymous namespace

scoped_refptr<StaticBitmapImage> GetImageWithAlphaDisposition(
    CanvasResourceProvider::FlushReason reason,
    scoped_refptr<StaticBitmapImage>&& image,
    const AlphaDisposition alpha_disposition) {
  if (!image)
    return nullptr;

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!paint_image)
    return nullptr;

  if (paint_image.GetAlphaType() == kOpaque_SkAlphaType ||
      alpha_disposition == kDontChangeAlpha)
    return std::move(image);

  const SkAlphaType alpha_type = (alpha_disposition == kPremultiplyAlpha)
                                     ? kPremul_SkAlphaType
                                     : kUnpremul_SkAlphaType;

  if (paint_image.GetAlphaType() == alpha_type) {
    return std::move(image);
  }

  SkImageInfo info = paint_image.GetSkImageInfo().makeAlphaType(alpha_type);

  // To premul, draw the unpremul image on a surface to avoid GPU read back if
  // image is texture backed.
  if (alpha_type == kPremul_SkAlphaType) {
    DCHECK_EQ(paint_image.GetAlphaType(), kUnpremul_SkAlphaType);
    auto resource_provider = CreateProvider(
        image->IsTextureBacked() ? image->ContextProviderWrapper() : nullptr,
        info, image, true /* fallback_to_software */);
    if (!resource_provider)
      return nullptr;

    cc::PaintFlags paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    resource_provider->Canvas()->drawImage(paint_image, 0, 0,
                                           SkSamplingOptions(), &paint);
    return resource_provider->Snapshot(reason,
                                       image->CurrentFrameOrientation());
  }

  // To unpremul, read back the pixels.
  // TODO(crbug.com/1197369): we should try to keep the output resource(image)
  // in GPU when premultiply-alpha or unpremultiply-alpha transforms is
  // required.
  DCHECK_EQ(alpha_type, kUnpremul_SkAlphaType);
  DCHECK_EQ(paint_image.GetAlphaType(), kPremul_SkAlphaType);
  if (paint_image.GetSkImageInfo().isEmpty())
    return nullptr;

  sk_sp<SkData> dst_pixels = TryAllocateSkData(info.computeMinByteSize());
  if (!dst_pixels)
    return nullptr;

  uint8_t* writable_pixels = static_cast<uint8_t*>(dst_pixels->writable_data());
  size_t image_row_bytes = static_cast<size_t>(info.minRowBytes64());
  bool read_successful =
      paint_image.readPixels(info, writable_pixels, image_row_bytes, 0, 0);
  DCHECK(read_successful);
  return StaticBitmapImage::Create(std::move(dst_pixels), info,
                                   image->CurrentFrameOrientation());
}

}  // namespace blink
