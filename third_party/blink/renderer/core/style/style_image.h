/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_IMAGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CSSValue;
class FloatSize;
class Image;
class ImageResourceContent;
class LayoutSize;
class SVGImage;
class Document;
class ComputedStyle;
class ImageResourceObserver;

typedef void* WrappedImagePtr;

// This class represents a CSS <image> value in ComputedStyle. The underlying
// object can be an image, a gradient or anything else defined as an <image>
// value.
class CORE_EXPORT StyleImage : public GarbageCollected<StyleImage> {
 public:
  virtual ~StyleImage() = default;

  bool operator==(const StyleImage& other) const { return IsEqual(other); }

  // Returns a CSSValue representing the origin <image> value. May not be the
  // actual CSSValue from which this StyleImage was originally created if the
  // CSSValue can be recreated easily (like for StyleFetchedImage) and does not
  // contain per-client state (like for StyleGeneratedImage.)
  virtual CSSValue* CssValue() const = 0;

  // Returns a CSSValue suitable for using as part of a computed style
  // value. This often means that any URLs have been made absolute, and similar
  // actions described by a "Computed value" in the relevant specification.
  virtual CSSValue* ComputedCSSValue(const ComputedStyle&,
                                     bool allow_visited_style) const = 0;

  // An Image can be provided for rendering by GetImage.
  virtual bool CanRender() const { return true; }

  // All underlying resources this <image> references has finished loading.
  virtual bool IsLoaded() const { return true; }

  // Any underlying resources this <image> references failed to load.
  virtual bool ErrorOccurred() const { return false; }

  // Determine the concrete object size of this <image>, scaled by multiplier,
  // using the specified default object size. Return value as a FloatSize
  // because we want integer sizes to remain integers when zoomed and then
  // unzoomed. That is, (size * multiplier) / multiplier == size.
  //
  // The default object size is context dependent, see for instance the
  // "Examples of CSS Object Sizing" section of the CSS images specification.
  // https://drafts.csswg.org/css-images/#sizing.
  //
  // The |default_object_size| is assumed to be in the effective zoom level
  // given by multiplier, i.e. if multiplier is 1 the |default_object_size| is
  // not zoomed. Note that the |default_object_size| has already been snapped
  // to LayoutUnit resolution because it represents the target painted size of
  // a container.
  virtual FloatSize ImageSize(const Document&,
                              float multiplier,
                              const LayoutSize& default_object_size) const = 0;

  // The <image> has intrinsic dimensions.
  //
  // If this returns false, then a call to ImageSize() is expected to return
  // the |default_object_size| argument that it was passed unmodified.
  virtual bool HasIntrinsicSize() const = 0;

  virtual void AddClient(ImageResourceObserver*) = 0;
  virtual void RemoveClient(ImageResourceObserver*) = 0;

  // Retrieve an Image representation for painting this <image>, at a particular
  // target size. Most often, the target size is a concrete object size
  // into which the image will be painted. But for background images the
  // target size is the area to be filled with a single copy of the image,
  // and can have a variety of relationships to the container's size. Hence
  // it requires float resolution.
  //
  // Note that the |target_size| is in the effective zoom level of the
  // computed style, i.e if the style has an effective zoom level of 1.0 the
  // |target_size| is not zoomed.
  virtual scoped_refptr<Image> GetImage(const ImageResourceObserver&,
                                        const Document&,
                                        const ComputedStyle&,
                                        const FloatSize& target_size) const = 0;

  // Opaque handle representing the underlying value of this <image>.
  virtual WrappedImagePtr Data() const = 0;

  // A scale factor indicating how many physical pixels in an image represent a
  // logical (CSS) pixel.
  virtual float ImageScaleFactor() const { return 1; }

  // Returns true if it can be determined that this <image> will always provide
  // an opaque Image.
  virtual bool KnownToBeOpaque(const Document&, const ComputedStyle&) const = 0;

  // If this <image> is intrinsically an image resource, this returns its
  // underlying ImageResourceContent, or otherwise nullptr.
  virtual ImageResourceContent* CachedImage() const { return nullptr; }

  ALWAYS_INLINE bool IsImageResource() const { return is_image_resource_; }
  ALWAYS_INLINE bool IsPendingImage() const { return is_pending_image_; }
  ALWAYS_INLINE bool IsGeneratedImage() const { return is_generated_image_; }
  ALWAYS_INLINE bool IsImageResourceSet() const {
    return is_image_resource_set_;
  }
  ALWAYS_INLINE bool IsPaintImage() const { return is_paint_image_; }

  bool IsLazyloadPossiblyDeferred() const {
    return is_lazyload_possibly_deferred_;
  }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  StyleImage()
      : is_image_resource_(false),
        is_pending_image_(false),
        is_generated_image_(false),
        is_image_resource_set_(false),
        is_paint_image_(false),
        is_lazyload_possibly_deferred_(false) {}
  bool is_image_resource_ : 1;
  bool is_pending_image_ : 1;
  bool is_generated_image_ : 1;
  bool is_image_resource_set_ : 1;
  bool is_paint_image_ : 1;
  bool is_lazyload_possibly_deferred_ : 1;

  virtual bool IsEqual(const StyleImage&) const = 0;

  FloatSize ApplyZoom(const FloatSize&, float multiplier) const;
  FloatSize ImageSizeForSVGImage(SVGImage*,
                                 float multiplier,
                                 const LayoutSize& default_object_size) const;
};

}  // namespace blink
#endif
