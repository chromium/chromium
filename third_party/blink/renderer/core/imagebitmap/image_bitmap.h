// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;
class ImageElementBase;
class ImageDecoder;
class OffscreenCanvas;
class ScriptPromiseResolver;

class CORE_EXPORT ImageBitmap final : public ScriptWrappable,
                                      public CanvasImageSource,
                                      public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Expects the ImageElementBase to return/have an SVGImage.
  static ScriptPromise CreateAsync(
      ImageElementBase*,
      absl::optional<gfx::Rect>,
      ScriptState*,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojom::blink::PreferredColorScheme,
      ExceptionState&,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static sk_sp<SkImage> GetSkImageFromDecoder(std::unique_ptr<ImageDecoder>);

  ImageBitmap(ImageElementBase*,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  ImageBitmap(HTMLVideoElement*,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  ImageBitmap(HTMLCanvasElement*,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  ImageBitmap(OffscreenCanvas*,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  ImageBitmap(ImageData*,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  ImageBitmap(ImageBitmap*,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  ImageBitmap(scoped_refptr<StaticBitmapImage>);
  ImageBitmap(scoped_refptr<StaticBitmapImage>,
              absl::optional<gfx::Rect>,
              const ImageBitmapOptions* = ImageBitmapOptions::Create());
  // This constructor may called by structured-cloning an ImageBitmap.
  // isImageBitmapOriginClean indicates whether the original ImageBitmap is
  // origin clean or not.
  ImageBitmap(const SkPixmap& pixmap,
              bool is_image_bitmap_origin_clean,
              ImageOrientationEnum);

  // Type and helper function required by CallbackPromiseAdapter:
  using WebType = sk_sp<SkImage>;
  static ImageBitmap* Take(ScriptPromiseResolver*, sk_sp<SkImage>);

  scoped_refptr<StaticBitmapImage> BitmapImage() const { return image_; }

  // Retrieve the SkImageInfo that best represents BitmapImage().
  SkImageInfo GetBitmapSkImageInfo() const;

  // When apply_orientation is true this method will orient the data according
  // to the source's EXIF information.
  Vector<uint8_t> CopyBitmapData(const SkImageInfo& info,
                                 bool apply_orientation);
  unsigned width() const;
  unsigned height() const;
  gfx::Size Size() const;

  bool IsNeutered() const override { return is_neutered_; }
  bool OriginClean() const { return image_->OriginClean(); }
  bool IsPremultiplied() const { return image_->IsPremultiplied(); }
  ImageOrientationEnum ImageOrientation() const {
    return image_->CurrentFrameOrientation().Orientation();
  }
  scoped_refptr<StaticBitmapImage> Transfer();
  void close();

  ~ImageBitmap() override;

  // CanvasImageSource implementation
  scoped_refptr<Image> GetSourceImageForCanvas(
      SourceImageStatus*,
      const gfx::SizeF&,
      const AlphaDisposition alpha_disposition = kPremultiplyAlpha) override;
  bool WouldTaintOrigin() const override {
    return image_ ? !image_->OriginClean() : false;
  }
  gfx::SizeF ElementSize(const gfx::SizeF&,
                         const RespectImageOrientationEnum) const override;
  bool IsImageBitmap() const override { return true; }
  bool IsAccelerated() const override;

  // ImageBitmapSource implementation
  gfx::Size BitmapSourceSize() const override { return Size(); }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  absl::optional<gfx::Rect>,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  struct ParsedOptions {
    bool flip_y = false;
    bool premultiply_alpha = true;
    bool should_scale_input = false;
    bool has_color_space_conversion = false;
    bool source_is_unpremul = false;
    unsigned resize_width = 0;
    unsigned resize_height = 0;
    gfx::Rect crop_rect;
    cc::PaintFlags::FilterQuality resize_quality =
        cc::PaintFlags::FilterQuality::kLow;
  };

 private:
  void UpdateImageBitmapMemoryUsage();
  static void ResolvePromiseOnOriginalThread(ScriptPromiseResolver*,
                                             bool origin_clean,
                                             std::unique_ptr<ParsedOptions>,
                                             sk_sp<SkImage>,
                                             const ImageOrientationEnum);
  static void RasterizeImageOnBackgroundThread(
      sk_sp<PaintRecord>,
      const gfx::Rect&,
      scoped_refptr<base::SequencedTaskRunner>,
      WTF::CrossThreadOnceFunction<void(sk_sp<SkImage>,
                                        const ImageOrientationEnum)> callback);
  scoped_refptr<StaticBitmapImage> image_;
  bool is_neutered_ = false;
  int32_t memory_usage_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_H_
