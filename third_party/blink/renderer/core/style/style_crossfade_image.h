// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CROSSFADE_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CROSSFADE_IMAGE_H_

#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {
class CSSCrossfadeValue;
}

// This class represents a <cross-fade()> <image> function.
class StyleCrossfadeImage final : public StyleImage {
 public:
  StyleCrossfadeImage(cssvalue::CSSCrossfadeValue&,
                      StyleImage* from_image,
                      StyleImage* to_image);
  ~StyleCrossfadeImage() override;

  CSSValue* CssValue() const override;
  CSSValue* ComputedCSSValue(const ComputedStyle&,
                             bool allow_visited_style) const override;

  bool CanRender() const override;
  bool IsLoading() const override;
  bool IsLoaded() const override;
  bool ErrorOccurred() const override;
  bool IsAccessAllowed(String&) const override;

  gfx::SizeF ImageSize(float multiplier,
                       const gfx::SizeF& default_object_size,
                       RespectImageOrientationEnum) const override;

  bool HasIntrinsicSize() const override;

  void AddClient(ImageResourceObserver*) override;
  void RemoveClient(ImageResourceObserver*) override;

  scoped_refptr<Image> GetImage(const ImageResourceObserver&,
                                const Document&,
                                const ComputedStyle&,
                                const gfx::SizeF& target_size) const override;

  WrappedImagePtr Data() const override;
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const override;

  void Trace(Visitor*) const override;

 private:
  bool IsEqual(const StyleImage&) const override;

  Member<cssvalue::CSSCrossfadeValue> original_value_;
  Member<StyleImage> from_image_;
  Member<StyleImage> to_image_;
};

template <>
struct DowncastTraits<StyleCrossfadeImage> {
  static bool AllowFrom(const StyleImage& style_image) {
    return style_image.IsCrossfadeImage();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_CROSSFADE_IMAGE_H_
