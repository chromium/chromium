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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_GENERATED_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_GENERATED_IMAGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSValue;
class CSSImageGeneratorValue;
class Document;
class ImageResourceObserver;

// This class represents a generated <image> such as a gradient, cross-fade or
// paint(...) function.
class CORE_EXPORT StyleGeneratedImage final : public StyleImage {
 public:
  explicit StyleGeneratedImage(const CSSImageGeneratorValue&);

  WrappedImagePtr Data() const override { return image_generator_value_.Get(); }

  CSSValue* CssValue() const override;
  CSSValue* ComputedCSSValue(const ComputedStyle&,
                             bool allow_visited_style) const override;

  FloatSize ImageSize(const Document&,
                      float multiplier,
                      const LayoutSize& default_object_size) const override;
  bool HasIntrinsicSize() const override { return fixed_size_; }
  void AddClient(ImageResourceObserver*) override;
  void RemoveClient(ImageResourceObserver*) override;
  // The |target_size| is the desired image size
  scoped_refptr<Image> GetImage(const ImageResourceObserver&,
                                const Document&,
                                const ComputedStyle&,
                                const FloatSize& target_size) const override;
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const override;

  bool IsUsingCustomProperty(const AtomicString& custom_property_name,
                             const Document&) const;

  void Trace(blink::Visitor*) override;

 private:
  bool IsEqual(const StyleImage&) const override;

  // TODO(sashab): Replace this with <const CSSImageGeneratorValue> once
  // Member<> supports const types.
  Member<CSSImageGeneratorValue> image_generator_value_;
  const bool fixed_size_;
};

template <>
struct DowncastTraits<StyleGeneratedImage> {
  static bool AllowFrom(const StyleImage& styleImage) {
    return styleImage.IsGeneratedImage();
  }
};

}  // namespace blink
#endif
