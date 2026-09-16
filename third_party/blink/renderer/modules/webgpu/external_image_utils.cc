// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/external_image_utils.h"

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/element_image.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/image_extractor.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

namespace blink {
namespace {

std::optional<ExternalImageSource> GetExternalImageSourceFromCanvasImageSource(
    CanvasImageSource* source,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  CHECK(!source->IsNeutered() && !source->IsPlaceholder());

  // Canvas element contains cross-origin data and may not be loaded
  if (source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "The external image is tainted by cross-origin data.");
    return {};
  }

  ExternalImageSource result;

  // HTMLCanvasElement and OffscreenCanvas won't care image orientation. But for
  // ImageBitmap, use kRespectImageOrientation will make ElementSize() behave
  // as Size().
  gfx::SizeF image_size = source->ElementSize(
      gfx::SizeF(),  // It will be ignored and won't affect size.
      kRespectImageOrientation);

  // The alpha op will happen at CopyTextureForBrowser() and
  // CopyContentFromCPU(). This will help combine more transforms (e.g. flipY,
  // color-space) into a single blit.
  // TODO(https://crbug.com/40760113): Ensure unpremultiplied images will live
  // on GPU if possible.
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  auto image_for_canvas =
      source->GetSourceImageForCanvas(&source_image_status, image_size);
  if (source_image_status != kNormalSourceImageStatus) {
    // Canvas back resource is broken, zero size, incomplete or invalid.
    // but developer can do nothing. Return nullptr and issue an noop.
    return {};
  }

  // TODO(https://crbug.com/40278208): It would be better if
  // GetSourceImageForCanvas() would always return a StaticBitmapImage.
  sk_sp<SkImage> sk_image = nullptr;
  bool image_is_default_orientation = image_for_canvas->HasDefaultOrientation();
  if (auto* image = DynamicTo<StaticBitmapImage>(image_for_canvas.get())) {
    if (image_is_default_orientation) {
      result.image = image;
    } else {
      // Handle non default orientation for StaticBitmapImage and ensure
      // it is not texture backed.
      sk_image = image->PaintImageForCurrentFrame().GetSwSkImage();

      if (!sk_image) {
        return {};
      }
    }
  } else {
    // HTMLImageElement input.
    // Below logic refs to ImageBitmap creation with ImageElementBase.
    // ImageExtractor recruit ImageDecoder to do decoder when:
    // - image is a BitmapImage, it usually happens when image contains coded
    // data.
    //   e.g. loaded image files *.png, *.jpg, *.bmp, *.ico, *.webp, *.avif,
    //   *.gif.
    // - alphaType, colorSpace are not equal to dst. Issuing a redecode to
    // generate
    //   required results.
    ImageExtractor image_extractor(
        image_for_canvas.get(),
        dst_info.premultiplied_alpha ? kPremul_SkAlphaType
                                     : kUnpremul_SkAlphaType,
        PredefinedColorSpaceToSkColorSpace(dst_info.color_space));
    sk_image = image_extractor.GetSkImage();

    if (!sk_image) {
      return {};
    }
    // It is possible that some HTMLImageElement contains content which cannot
    // be decoded. e.g svg files. Using this path to handle them by converting
    // it to SkBitmap first and raster it.
    if (sk_image->isLazyGenerated()) {
      SkBitmap bitmap;
      auto image_info = sk_image->imageInfo();
      bitmap.allocPixels(image_info, image_info.minRowBytes());
      if (!sk_image->readPixels(bitmap.pixmap(), 0, 0)) {
        return {};
      }

      sk_image = SkImages::RasterFromBitmap(bitmap);
    }
  }

  if (sk_image) {
    CHECK(!result.image);

    // Create UnacceleratedStaticBitmapImage to create a most suitable
    // PaintImageBuilder. Use the builder to create PaintImage internally.
    // Store the orientation metadata but no transforms apply to the content.
    auto image = UnacceleratedStaticBitmapImage::Create(
        std::move(sk_image), image_for_canvas->Orientation());

    // Recruit Image::ResizeAndOrientImage() to apply transformation based on
    // orientation metadata. This API helps rotate contents based on orientation
    // metadata. After the transformation, reading content in default
    // orientation get the transformed results. Recreate unaccelerated static
    // bitmap with the transformed content with default orientation for post
    // processing.
    if (!image_is_default_orientation) {
      PaintImage paint_image = image->PaintImageForCurrentFrame();
      paint_image = Image::ResizeAndOrientImage(
          paint_image, image_for_canvas->Orientation(), gfx::Vector2dF(1, 1), 1,
          kInterpolationNone);

      // Have default orientation now.
      image = UnacceleratedStaticBitmapImage::Create(std::move(paint_image));
    }

    result.image = image;
  }

  result.width = static_cast<uint32_t>(result.image->width());
  result.height = static_cast<uint32_t>(result.image->height());
  return {result};
}

std::optional<ExternalImageSource>
GetExternalImageSourceFromCanvasRenderingContextHost(
    CanvasRenderingContextHost* canvas,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  if (!(canvas->IsWebGL() || canvas->IsRenderingContext2D() ||
        canvas->IsWebGPU() || canvas->IsImageBitmapRenderingContext())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "CopyExternalImageToTexture doesn't support canvas without rendering "
        "context");
    return {};
  }

  return GetExternalImageSourceFromCanvasImageSource(canvas, dst_info,
                                                     exception_state);
}

static constexpr uint64_t kDawnBytesPerRowAlignmentBits = 8;

// Calculate bytes per row for T2B/B2T copy
// TODO(shaobo.yan@intel.com): Using Dawn's constants once they are exposed
uint64_t AlignBytesPerRow(uint64_t bytesPerRow) {
  return (((bytesPerRow - 1) >> kDawnBytesPerRowAlignmentBits) + 1)
         << kDawnBytesPerRowAlignmentBits;
}

wgpu::TextureFormat SkColorTypeToDawnColorFormat(SkColorType sk_color_type) {
  switch (sk_color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      return wgpu::TextureFormat::RGBA8Unorm;
    case SkColorType::kBGRA_8888_SkColorType:
      return wgpu::TextureFormat::BGRA8Unorm;
    case SkColorType::kRGBA_F16_SkColorType:
      return wgpu::TextureFormat::RGBA16Float;
    default: {
      NOTREACHED();
    }
  }
}

wgpu::TextureFormat VizToWGPUFormat(const viz::SharedImageFormat& format) {
  // This function provides the inverse mapping of `WGPUFormatToViz` (located in
  // webgpu_swap_buffer_provider.cc).
  if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return wgpu::TextureFormat::BGRA8Unorm;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return wgpu::TextureFormat::RGBA8Unorm;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return wgpu::TextureFormat::RGBA16Float;
  }
  NOTREACHED() << "Unexpected canvas format: " << format.ToString();
}

// CopyExternalImageToTexture() needs to set src/dst AlphaMode, flipY and color
// space conversion related params. This helper function also initializes
// ColorSpaceConversionConstants param.
wgpu::CopyTextureForBrowserOptions CreateCopyTextureForBrowserOptions(
    const StaticBitmapImage* image,
    const PaintImage* paint_image,
    PredefinedColorSpace dst_color_space,
    bool dst_premultiplied_alpha,
    bool flipY,
    ColorSpaceConversionConstants* color_space_conversion_constants) {
  wgpu::CopyTextureForBrowserOptions options = {
      .srcAlphaMode = image->IsPremultiplied()
                          ? wgpu::AlphaMode::Premultiplied
                          : wgpu::AlphaMode::Unpremultiplied,
      .dstAlphaMode = dst_premultiplied_alpha
                          ? wgpu::AlphaMode::Premultiplied
                          : wgpu::AlphaMode::Unpremultiplied,
  };

  // Set color space conversion params
  sk_sp<SkColorSpace> sk_src_color_space =
      paint_image->GetSkImageInfo().refColorSpace();

  // If source input discard the color space info(e.g. ImageBitmap created with
  // flag colorSpaceConversion: none). Treat the source color space as sRGB.
  if (sk_src_color_space == nullptr) {
    sk_src_color_space = SkColorSpace::MakeSRGB();
  }

  gfx::ColorSpace gfx_src_color_space = gfx::ColorSpace(*sk_src_color_space);
  gfx::ColorSpace gfx_dst_color_space =
      PredefinedColorSpaceToGfxColorSpace(dst_color_space);

  *color_space_conversion_constants = GetColorSpaceConversionConstants(
      gfx_src_color_space, gfx_dst_color_space);

  if (gfx_src_color_space != gfx_dst_color_space) {
    options.needsColorSpaceConversion = true;
    options.srcTransferFunctionParameters =
        color_space_conversion_constants->src_transfer_constants.data();
    options.dstTransferFunctionParameters =
        color_space_conversion_constants->dst_transfer_constants.data();
    options.conversionMatrix =
        color_space_conversion_constants->gamut_conversion_matrix.data();
  }
  // The source texture, which is either a WebGPUMailboxTexture for
  // accelerated images or an intermediate texture created for unaccelerated
  // images, is always origin top left, so no additional flip is needed apart
  // from the client specified flip in GPUImageCopyExternalImage i.e. |flipY|.
  options.flipY = flipY;

  return options;
}

// Helper function to get clipped rect from source image. Using in
// CopyExternalImageToTexture().
gfx::Rect GetSourceImageSubrect(StaticBitmapImage* image,
                                gfx::Rect source_image_rect,
                                const wgpu::Origin2D& origin,
                                const wgpu::Extent3D& copy_size) {
  int width = static_cast<int>(copy_size.width);
  int height = static_cast<int>(copy_size.height);
  int x = static_cast<int>(origin.x) + source_image_rect.x();
  int y = static_cast<int>(origin.y) + source_image_rect.y();

  // Ensure generated source image subrect is into source image rect.
  CHECK(width <= source_image_rect.width() - source_image_rect.x() &&
        height <= source_image_rect.height() - source_image_rect.y() &&
        x <= source_image_rect.width() - source_image_rect.x() - width &&
        y <= source_image_rect.height() - source_image_rect.y() - height);

  return gfx::Rect(x, y, width, height);
}
}  // anonymous namespace

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    HTMLVideoElement* video,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  auto external_texture_source =
      GetExternalTextureSourceFromVideoElement(video, exception_state);
  if (!external_texture_source.valid) {
    return {};
  }
  CHECK(external_texture_source.media_video_frame);

  ExternalImageSource result;

  // Use display size to handle rotated video frame.
  auto natural_size = external_texture_source.media_video_frame->natural_size();
  const auto transform = external_texture_source.media_video_frame->metadata()
                             .transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    result.width = static_cast<uint32_t>(natural_size.width());
    result.height = static_cast<uint32_t>(natural_size.height());
  } else {
    result.width = static_cast<uint32_t>(natural_size.height());
    result.height = static_cast<uint32_t>(natural_size.width());
  }

  result.external_texture_source = std::move(external_texture_source);
  return {result};
}

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    ImageData* image_data,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  // TODO(https://crbug.com/40278208): Avoid extra copy.
  SkPixmap image_data_pixmap = image_data->GetSkPixmap();
  SkImageInfo info = image_data_pixmap.info().makeColorType(kN32_SkColorType);
  size_t image_pixels_size = info.computeMinByteSize();
  if (SkImageInfo::ByteSizeOverflowed(image_pixels_size)) {
    return {};
  }
  sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
  if (!image_pixels) {
    return {};
  }
  if (!image_data_pixmap.readPixels(info, image_pixels->writable_data(),
                                    info.minRowBytes(), 0, 0)) {
    return {};
  }

  ExternalImageSource result;
  result.image = StaticBitmapImage::Create(std::move(image_pixels), info,
                                           gfx::HDRMetadata());
  result.width = static_cast<uint32_t>(image_data->width());
  result.height = static_cast<uint32_t>(image_data->height());
  return {result};
}

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    VideoFrame* frame,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  auto external_texture_source =
      GetExternalTextureSourceFromVideoFrame(frame, exception_state);
  if (!external_texture_source.valid) {
    return {};
  }
  CHECK(external_texture_source.media_video_frame);

  ExternalImageSource result;
  result.external_texture_source = external_texture_source;
  result.width = frame->displayWidth();
  result.height = frame->displayHeight();
  return {result};
}

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    HTMLCanvasElement* canvas,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  if (canvas->IsPlaceholder()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot copy from a canvas that has had "
                                      "transferControlToOffscreen() called.");
    return {};
  }

  return GetExternalImageSourceFromCanvasRenderingContextHost(canvas, dst_info,
                                                              exception_state);
}

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    OffscreenCanvas* canvas,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  if (canvas->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas has been transferred.");
    return {};
  }

  return GetExternalImageSourceFromCanvasRenderingContextHost(canvas, dst_info,
                                                              exception_state);
}

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    HTMLImageElement* image,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  return GetExternalImageSourceFromCanvasImageSource(image, dst_info,
                                                     exception_state);
}

std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    ImageBitmap* image,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state) {
  if (image->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ImageBitmap has been detached.");
    return {};
  }

  return GetExternalImageSourceFromCanvasImageSource(image, dst_info,
                                                     exception_state);
}

bool CopyStaticImageBitmapToWGPUTexture(
    const scoped_refptr<DawnControlClientHolder>& dawn_control_client,
    const wgpu::Device& device,
    StaticBitmapImage* image,
    const wgpu::Origin2D& origin,
    const wgpu::Extent3D& copy_size,
    const wgpu::TexelCopyTextureInfo& destination,
    bool dst_premultiplied_alpha,
    PredefinedColorSpace dst_color_space,
    bool flipY) {
  // If GPU backed image failed to uploading through GPU, call
  // MakeUnaccelerated() to generate CPU backed image and fallback to CPU
  // uploading path.
  scoped_refptr<StaticBitmapImage> unaccelerated_image = nullptr;
  bool use_webgpu_mailbox_texture = true;

// TODO(crbug.com/1309194): using webgpu mailbox texture uploading path on linux
// platform requires interop supported. According to the bug, this change will
// be a long time task. So disable using webgpu mailbox texture uploading path
// on linux platform.
// TODO(crbug.com/1424119): using a webgpu mailbox texture on the OpenGLES
// backend is failing for unknown reasons.
#if BUILDFLAG(IS_LINUX)
  bool forceReadback = true;
#elif BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/dawn/1969): Some Android devices don't fail to copy from
  // ImageBitmaps that were created from a non-texture-backed source, like
  // ImageData. Forcing those textures down the readback path is an easy way to
  // ensure the copies succeed. May be able to remove this check with some
  // better synchronization in the future.
  bool forceReadback = !image->IsTextureBacked();
#elif BUILDFLAG(IS_WIN)
  wgpu::AdapterInfo adapter_info = {};
  device.GetAdapter().GetInfo(&adapter_info);
  bool forceReadback = adapter_info.backendType == wgpu::BackendType::OpenGLES;
#else
  bool forceReadback = false;
#endif
  if (forceReadback) {
    use_webgpu_mailbox_texture = false;
    unaccelerated_image = image->MakeUnaccelerated();
    image = unaccelerated_image.get();
  }

  bool noop = copy_size.width == 0 || copy_size.height == 0 ||
              copy_size.depthOrArrayLayers == 0;

  // The copy rect might be a small part from a large source image. Instead of
  // copying the whole large source image, clipped to the small rect and upload
  // it is more performant. The clip rect should be chosen carefully when a
  // flipY op is required during uploading.
  gfx::Rect image_source_copy_rect =
      GetSourceImageSubrect(image, image->Rect(), origin, copy_size);

  // Get source image info.
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  SkImageInfo source_image_info = paint_image.GetSkImageInfo();

  // TODO(crbug.com/1457649): If CPU backed source input discard the color
  // space info(e.g. ImageBitmap created with flag colorSpaceConversion: none).
  // disable using use_webgpu_mailbox_texture to fix alpha premultiplied isseu.
  if (!image->IsTextureBacked() && !image->IsPremultiplied() &&
      source_image_info.refColorSpace() == nullptr) {
    use_webgpu_mailbox_texture = false;
  }

  // Source and dst might have different constants
  ColorSpaceConversionConstants color_space_conversion_constants = {};

  // This uploading path try to extract WebGPU mailbox texture from source
  // image based on the copy size.
  // The uploading path works like this:
  // - Try to get WebGPUMailboxTexture with image source copy rect.
  // - If success, Issue Dawn::queueCopyTextureForBrowser to upload contents
  //   to WebGPU texture.
  if (use_webgpu_mailbox_texture) {
    if (image->IsTextureBacked()) {
      auto* accelerated_image =
          static_cast<AcceleratedStaticBitmapImage*>(image);
      if (accelerated_image->GetSharedImage()->usage().Has(
              gpu::SHARED_IMAGE_USAGE_WEBGPU_READ)) {
        wgpu::TextureDescriptor texture_desc = {
            .usage = wgpu::TextureUsage::CopySrc |
                     wgpu::TextureUsage::TextureBinding,
            .size = {base::checked_cast<uint32_t>(source_image_info.width()),
                     base::checked_cast<uint32_t>(source_image_info.height()),
                     1},
            .format = VizToWGPUFormat(image->GetSharedImageFormat()),
        };

        scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
            WebGPUMailboxTexture::FromExistingSharedImage(
                dawn_control_client, device, texture_desc,
                accelerated_image->GetSharedImage(),
                accelerated_image->GetSyncToken());

        wgpu::TexelCopyTextureInfo src = {
            .texture = mailbox_texture->GetTexture(),
            .origin = {
                .x = base::checked_cast<uint32_t>(image_source_copy_rect.x()),
                .y = base::checked_cast<uint32_t>(image_source_copy_rect.y())}};

        wgpu::CopyTextureForBrowserOptions options =
            CreateCopyTextureForBrowserOptions(
                image, &paint_image, dst_color_space, dst_premultiplied_alpha,
                flipY, &color_space_conversion_constants);

        device.GetQueue().CopyTextureForBrowser(&src, &destination, &copy_size,
                                                &options);

        accelerated_image->UpdateSyncToken(mailbox_texture->Dissociate());
        return true;
      }
    }

    // The copy rect might be a small part from a large source image. Instead of
    // copying large source image, clipped to the small copy rect is more
    // performant. The clip rect should be chosen carefully when a flipY op is
    // required during uploading.
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromStaticBitmapImage(
            dawn_control_client, device,
            wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc |
                wgpu::TextureUsage::TextureBinding,
            image, image_source_copy_rect, noop);

    if (mailbox_texture != nullptr) {
      wgpu::TexelCopyTextureInfo src = {.texture =
                                            mailbox_texture->GetTexture()};

      wgpu::CopyTextureForBrowserOptions options =
          CreateCopyTextureForBrowserOptions(
              image, &paint_image, dst_color_space, dst_premultiplied_alpha,
              flipY, &color_space_conversion_constants);

      device.GetQueue().CopyTextureForBrowser(&src, &destination, &copy_size,
                                              &options);
      return true;
    }
    // Fallback path accepts CPU backed resource only.
    unaccelerated_image = image->MakeUnaccelerated();
    image = unaccelerated_image.get();
    paint_image = image->PaintImageForCurrentFrame();
    image_source_copy_rect =
        GetSourceImageSubrect(image, image->Rect(), origin, copy_size);
    source_image_info = paint_image.GetSkImageInfo();
  }

  // This fallback path will handle all cases that cannot extract source image
  // to webgpu mailbox texture based on copy rect. It accepts CPU backed
  // resource only. The fallback path works like this:
  // - Always create a mappable wgpu::Buffer and copy CPU backed image resource
  // to the buffer.
  // - Always create a wgpu::Texture and issue a B2T copy to upload the content
  // from buffer to texture.
  // - Issue Dawn::queueCopyTextureForBrowser to upload contents from temp
  // texture to dst texture.
  // - Destroy all temp resources.
  CHECK(!image->IsTextureBacked());
  CHECK(!paint_image.IsTextureBacked());

  // Handling CPU resource.

  // Create intermediate texture as input for CopyTextureForBrowser().
  // For noop copy, creating intermediate texture with minimum size.
  const uint32_t src_width =
      noop && image_source_copy_rect.width() == 0
          ? 1
          : static_cast<uint32_t>(image_source_copy_rect.width());
  const uint32_t src_height =
      noop && image_source_copy_rect.height() == 0
          ? 1
          : static_cast<uint32_t>(image_source_copy_rect.height());

  SkColorType source_color_type = source_image_info.colorType();
  wgpu::TextureDescriptor texture_desc = {
      .usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
               wgpu::TextureUsage::TextureBinding,
      .size = {src_width, src_height, 1},
      .format = SkColorTypeToDawnColorFormat(source_color_type),
  };

  wgpu::Texture intermediate_texture = device.CreateTexture(&texture_desc);

  // For noop copy, read source image content to mappable webgpu buffer and
  // using B2T copy to copy source content to intermediate texture.
  if (!noop) {
    // Source type is SkColorType::kRGBA_8888_SkColorType or
    // SkColorType::kBGRA_8888_SkColorType.
    uint64_t bytes_per_pixel = 4;

    base::CheckedNumeric<uint32_t> bytes_per_row =
        AlignBytesPerRow(image_source_copy_rect.width() * bytes_per_pixel);

    // Static cast to uint64_t to catch overflow during multiplications and use
    // base::CheckedNumeric to catch this overflow.
    base::CheckedNumeric<size_t> size_in_bytes =
        bytes_per_row * static_cast<uint64_t>(image_source_copy_rect.height());

    // Overflow happens when calculating size or row bytes.
    if (!size_in_bytes.IsValid()) {
      return false;
    }

    uint32_t wgpu_bytes_per_row = bytes_per_row.ValueOrDie();

    // Create a mapped buffer to receive external image contents
    wgpu::BufferDescriptor buffer_desc = {
        .usage = wgpu::BufferUsage::CopySrc,
        .size = size_in_bytes.ValueOrDie(),
        .mappedAtCreation = true,
    };

    wgpu::Buffer intermediate_buffer = device.CreateBuffer(&buffer_desc);

    // This could happen either on OOM or if the image is to large to fit the
    // size in a uint32.
    if (!intermediate_buffer) {
      return false;
    }

    size_t size = static_cast<size_t>(buffer_desc.size);
    void* data = intermediate_buffer.GetMappedRange(0, size);

    // SAFETY: Mapped Range already checked
    auto dest_pixels = data != nullptr ? UNSAFE_BUFFERS(base::span<uint8_t>(
                                             static_cast<uint8_t*>(data), size))
                                       : base::span<uint8_t>();

    SkImageInfo copy_rect_info = source_image_info.makeWH(
        image_source_copy_rect.width(), image_source_copy_rect.height());
    bool success = paint_image.readPixels(
        copy_rect_info, dest_pixels.data(), wgpu_bytes_per_row,
        image_source_copy_rect.x(), image_source_copy_rect.y());
    if (!success) {
      return false;
    }

    intermediate_buffer.Unmap();

    // Start a B2T copy to move contents from buffer to intermediate texture
    wgpu::TexelCopyBufferInfo dawn_intermediate_buffer = {
        .layout =
            {
                .bytesPerRow = wgpu_bytes_per_row,
                .rowsPerImage = copy_size.height,
            },
        .buffer = intermediate_buffer,
    };

    wgpu::TexelCopyTextureInfo dawn_intermediate_texture = {
        .texture = intermediate_texture,
        .aspect = wgpu::TextureAspect::All,
    };

    wgpu::Extent3D source_image_copy_size = {copy_size.width, copy_size.height};

    wgpu::CommandEncoderDescriptor command_encoder_desc = {
        .label = "GPUQueue::CopyFromCanvasSourceImage",
    };

    wgpu::CommandEncoder encoder =
        device.CreateCommandEncoder(&command_encoder_desc);

    encoder.CopyBufferToTexture(&dawn_intermediate_buffer,
                                &dawn_intermediate_texture,
                                &source_image_copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    device.GetQueue().Submit(1, &commands);
  }

  wgpu::TexelCopyTextureInfo src = {
      .texture = intermediate_texture,
  };
  wgpu::CopyTextureForBrowserOptions options =
      CreateCopyTextureForBrowserOptions(image, &paint_image, dst_color_space,
                                         dst_premultiplied_alpha, flipY,
                                         &color_space_conversion_constants);
  device.GetQueue().CopyTextureForBrowser(&src, &destination, &copy_size,
                                          &options);
  return true;
}
}  // namespace blink
