// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_MASK_REFERENCE_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_MASK_REFERENCE_IMAGE_H_

#include "third_party/blink/renderer/core/style/style_image.h"

#include "third_party/blink/renderer/core/svg/proxy_svg_resource_client.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSImageValue;
class SVGResource;
class StyleSVGResource;

class StyleSVGMaskReferenceImage : public StyleImage {
 public:
  StyleSVGMaskReferenceImage(SVGResource* resource,
                             CSSImageValue* resource_css_value);
  ~StyleSVGMaskReferenceImage() override;

  CSSValue* CssValue() const override;
  CSSValue* ComputedCSSValue(const ComputedStyle&,
                             bool allow_visited_style) const override;

  bool IsAccessAllowed(String& failing_url) const override;

  IntrinsicSizingInfo GetNaturalSizingInfo(
      float multiplier,
      RespectImageOrientationEnum) const override;

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

  SVGResource* GetSVGResource() const;
  ProxySVGResourceClient& GetSVGResourceClient() const;

  void Trace(Visitor* visitor) const override;

  StyleSVGResource* CreateSVGResourceWrapper();

 private:
  bool IsEqual(const StyleImage&) const override;

  Member<SVGResource> resource_;
  Member<CSSImageValue> resource_css_value_;
};

template <>
struct DowncastTraits<StyleSVGMaskReferenceImage> {
  static bool AllowFrom(const StyleImage& style_image) {
    return style_image.IsSVGMaskReference();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SVG_MASK_REFERENCE_IMAGE_H_
