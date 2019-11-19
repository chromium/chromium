// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/canvas/image_element_base.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {
class Document;
class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;
class ImageDecoder;
class OffscreenCanvas;

enum ImageBitmapPixelFormat {
  kImageBitmapPixelFormat_Default,
  kImageBitmapPixelFormat_Uint8,
};

class CORE_EXPORT ImageBitmap final : public ScriptWrappable,
                                      public CanvasImageSource,
                                      public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageBitmap* Create(
      ImageElementBase*,
      base::Optional<IntRect>,
      Document*,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static ImageBitmap* Create(
      HTMLVideoElement*,
      base::Optional<IntRect>,
      Document*,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static ImageBitmap* Create(
      HTMLCanvasElement*,
      base::Optional<IntRect>,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static ImageBitmap* Create(
      OffscreenCanvas*,
      base::Optional<IntRect>,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static ImageBitmap* Create(
      ImageData*,
      base::Optional<IntRect>,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static ImageBitmap* Create(
      ImageBitmap*,
      base::Optional<IntRect>,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static ImageBitmap* Create(scoped_refptr<StaticBitmapImage>);
  static ImageBitmap* Create(
      scoped_refptr<StaticBitmapImage>,
      base::Optional<IntRect>,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  // This function is called by structured-cloning an ImageBitmap.
  // isImageBitmapPremultiplied indicates whether the original ImageBitmap is
  // premultiplied or not.
  // isImageBitmapOriginClean indicates whether the original ImageBitmap is
  // origin clean or not.
  static ImageBitmap* Create(const void* pixel_data,
                             uint32_t width,
                             uint32_t height,
                             bool is_image_bitmap_premultiplied,
                             bool is_image_bitmap_origin_clean,
                             const CanvasColorParams&);
  static ScriptPromise CreateAsync(
      ImageElementBase*,
      base::Optional<IntRect>,
      Document*,
      ScriptState*,
      const ImageBitmapOptions* = ImageBitmapOptions::Create());
  static sk_sp<SkImage> GetSkImageFromDecoder(std::unique_ptr<ImageDecoder>);

  ImageBitmap(ImageElementBase*,
              base::Optional<IntRect>,
              Document*,
              const ImageBitmapOptions*);
  ImageBitmap(HTMLVideoElement*,
              base::Optional<IntRect>,
              Document*,
              const ImageBitmapOptions*);
  ImageBitmap(HTMLCanvasElement*,
              base::Optional<IntRect>,
              const ImageBitmapOptions*);
  ImageBitmap(OffscreenCanvas*,
              base::Optional<IntRect>,
              const ImageBitmapOptions*);
  ImageBitmap(ImageData*, base::Optional<IntRect>, const ImageBitmapOptions*);
  ImageBitmap(ImageBitmap*, base::Optional<IntRect>, const ImageBitmapOptions*);
  ImageBitmap(scoped_refptr<StaticBitmapImage>);
  ImageBitmap(scoped_refptr<StaticBitmapImage>,
              base::Optional<IntRect>,
              const ImageBitmapOptions*);
  ImageBitmap(const void* pixel_data,
              uint32_t width,
              uint32_t height,
              bool is_image_bitmap_premultiplied,
              bool is_image_bitmap_origin_clean,
              const CanvasColorParams&);

  // Type and helper function required by CallbackPromiseAdapter:
  using WebType = sk_sp<SkImage>;
  static ImageBitmap* Take(ScriptPromiseResolver*, sk_sp<SkImage>);

  scoped_refptr<StaticBitmapImage> BitmapImage() const { return image_; }
  Vector<uint8_t> CopyBitmapData();
  Vector<uint8_t> CopyBitmapData(AlphaDisposition,
                                 DataU8ColorType = kRGBAColorType);
  unsigned width() const;
  unsigned height() const;
  IntSize Size() const;

  bool IsNeutered() const { return is_neutered_; }
  bool OriginClean() const { return image_->OriginClean(); }
  bool IsPremultiplied() const { return image_->IsPremultiplied(); }
  scoped_refptr<StaticBitmapImage> Transfer();
  void close();

  ~ImageBitmap() override;

  CanvasColorParams GetCanvasColorParams();

  // CanvasImageSource implementation
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               const FloatSize&) override;
  bool WouldTaintOrigin() const override { return !image_->OriginClean(); }
  void AdjustDrawRects(FloatRect* src_rect, FloatRect* dst_rect) const override;
  FloatSize ElementSize(const FloatSize&) const override;
  bool IsImageBitmap() const override { return true; }
  bool IsAccelerated() const override;

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override { return Size(); }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  base::Optional<IntRect>,
                                  const ImageBitmapOptions*) override;

  struct ParsedOptions {
    bool flip_y = false;
    bool premultiply_alpha = true;
    bool should_scale_input = false;
    bool has_color_space_conversion = false;
    bool preserve_source_color_space = false;
    bool source_is_unpremul = false;
    unsigned resize_width = 0;
    unsigned resize_height = 0;
    IntRect crop_rect;
    ImageBitmapPixelFormat pixel_format = kImageBitmapPixelFormat_Default;
    SkFilterQuality resize_quality = kLow_SkFilterQuality;
    CanvasColorParams color_params;
  };

 private:
  void UpdateImageBitmapMemoryUsage();
  static void ResolvePromiseOnOriginalThread(ScriptPromiseResolver*,
                                             bool origin_clean,
                                             std::unique_ptr<ParsedOptions>,
                                             sk_sp<SkImage>);
  static void RasterizeImageOnBackgroundThread(
      sk_sp<PaintRecord>,
      const IntRect&,
      scoped_refptr<base::SequencedTaskRunner>,
      WTF::CrossThreadOnceFunction<void(sk_sp<SkImage>)> callback);
  scoped_refptr<StaticBitmapImage> image_;
  bool is_neutered_ = false;
  int32_t memory_usage_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_H_
