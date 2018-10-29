// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PLACEHOLDER_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PLACEHOLDER_IMAGE_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class FloatPoint;
class FloatRect;
class FloatSize;
class GraphicsContext;
class ImageObserver;

// A generated placeholder image that shows a translucent gray rectangle with
// the full resource size (for example, 100KB) shown in the center. For
// LazyImages the placeholder image will be a plain translucent rectangle.
class PLATFORM_EXPORT PlaceholderImage final : public Image {
 public:
  static scoped_refptr<PlaceholderImage> Create(
      ImageObserver* observer,
      const IntSize& size,
      int64_t original_resource_size) {
    return base::AdoptRef(
        new PlaceholderImage(observer, size, original_resource_size, false));
  }

  static scoped_refptr<PlaceholderImage> CreateForLazyImages(
      ImageObserver* observer,
      const IntSize& size) {
    return base::AdoptRef(new PlaceholderImage(observer, size, 0, true));
  }

  ~PlaceholderImage() override;

  IntSize Size() const override;

  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const FloatRect& dest_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;

  void DestroyDecodedData() override;

  PaintImage PaintImageForCurrentFrame() override;

  bool IsPlaceholderImage() const override;

  const String& GetTextForTesting() const { return text_; }

 private:
  PlaceholderImage(ImageObserver*,
                   const IntSize&,
                   int64_t original_resource_size,
                   bool is_lazy_image);

  bool CurrentFrameHasSingleSecurityOrigin() const override;

  bool CurrentFrameKnownToBeOpaque() override;

  void DrawPattern(GraphicsContext&,
                   const FloatRect& src_rect,
                   const FloatSize& scale,
                   const FloatPoint& phase,
                   SkBlendMode,
                   const FloatRect& dest_rect,
                   const FloatSize& repeat_spacing) override;

  // SetData does nothing, and the passed in buffer is ignored.
  SizeAvailability SetData(scoped_refptr<SharedBuffer>, bool) override;

  const IntSize size_;
  const String text_;

  // This placeholder image is used for lazyloading of images.
  bool is_lazy_image_;

  class SharedFont;
  // Lazily initialized. All instances of PlaceholderImage will share the same
  // Font object, wrapped as a SharedFont.
  scoped_refptr<SharedFont> shared_font_;

  // Lazily initialized.
  base::Optional<float> cached_text_width_;
  sk_sp<PaintRecord> paint_record_for_current_frame_;
  PaintImage::ContentId paint_record_content_id_;
};

}  // namespace blink

#endif
