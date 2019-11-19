/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PENDING_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_PENDING_IMAGE_H_

#include "third_party/blink/renderer/core/css/css_image_generator_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ImageResourceObserver;

// StylePendingImage is a placeholder StyleImage that is entered into the
// ComputedStyle during style resolution, in order to avoid loading images that
// are not referenced by the final style.  They should never exist in a
// ComputedStyle after it has been returned from the style selector.
class StylePendingImage final : public StyleImage {
 public:
  explicit StylePendingImage(const CSSValue& value)
      : value_(const_cast<CSSValue*>(&value)) {
    is_pending_image_ = true;
  }

  WrappedImagePtr Data() const override { return value_.Get(); }

  CSSValue* CssValue() const override { return value_; }

  CSSValue* ComputedCSSValue(const ComputedStyle&,
                             bool allow_visited_style) const override {
    NOTREACHED();
    return nullptr;
  }

  CSSImageValue* CssImageValue() const {
    return DynamicTo<CSSImageValue>(value_.Get());
  }
  CSSPaintValue* CssPaintValue() const {
    return DynamicTo<CSSPaintValue>(value_.Get());
  }
  CSSImageGeneratorValue* CssImageGeneratorValue() const {
    return DynamicTo<CSSImageGeneratorValue>(value_.Get());
  }
  CSSImageSetValue* CssImageSetValue() const {
    return DynamicTo<CSSImageSetValue>(value_.Get());
  }

  FloatSize ImageSize(const Document&,
                      float /*multiplier*/,
                      const LayoutSize& /*defaultObjectSize*/) const override {
    return FloatSize();
  }
  bool HasIntrinsicSize() const override { return true; }
  void AddClient(ImageResourceObserver*) override {}
  void RemoveClient(ImageResourceObserver*) override {}
  scoped_refptr<Image> GetImage(const ImageResourceObserver&,
                                const Document&,
                                const ComputedStyle&,
                                const FloatSize& target_size) const override {
    NOTREACHED();
    return nullptr;
  }
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const override {
    return false;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(value_);
    StyleImage::Trace(visitor);
  }

 private:
  bool IsEqual(const StyleImage& other) const override;

  // TODO(sashab): Replace this with <const CSSValue> once Member<>
  // supports const types.
  Member<CSSValue> value_;
};

template <>
struct DowncastTraits<StylePendingImage> {
  static bool AllowFrom(const StyleImage& styleImage) {
    return styleImage.IsPendingImage();
  }
};

inline bool StylePendingImage::IsEqual(const StyleImage& other) const {
  if (!other.IsPendingImage())
    return false;
  const auto& other_pending = To<StylePendingImage>(other);
  return value_ == other_pending.value_;
}

}  // namespace blink
#endif
