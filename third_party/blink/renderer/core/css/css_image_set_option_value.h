// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_SET_OPTION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_SET_OPTION_VALUE_H_

#include "third_party/blink/renderer/core/css/css_image_set_type_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This class represents an image-set-option as specified in:
// https://w3c.github.io/csswg-drafts/css-images-4/#typedef-image-set-option
// <image-set-option> = [ <image> | <string> ] [<resolution> || type(<string>)]
class CSSImageSetOptionValue : public CSSValue {
 public:
  explicit CSSImageSetOptionValue(
      const CSSValue* image,
      const CSSNumericLiteralValue* resolution = nullptr,
      const CSSImageSetTypeValue* type = nullptr);

  // It is expected that CSSImageSetOptionValue objects should always have
  // non-null image and resolution values.
  CSSImageSetOptionValue() = delete;

  ~CSSImageSetOptionValue();

  StyleImage* CacheImage(
      const Document& document,
      const FetchParameters::ImageRequestBehavior image_request_behavior,
      const CrossOriginAttributeValue cross_origin,
      const CSSToLengthConversionData::ContainerSizes& container_sizes) const;

  // Gets the resolution value in Dots Per Pixel
  double ComputedResolution() const;

  // Returns true if the image-set-option uses an image format that the
  // browser can render.
  bool IsSupported() const;

  String CustomCSSText() const;

  bool Equals(const CSSImageSetOptionValue& other) const;

  CSSImageSetOptionValue* ComputedCSSValue(
      const ComputedStyle& style,
      const bool allow_visited_style) const;

  void TraceAfterDispatch(blink::Visitor* visitor) const;

 private:
  Member<const CSSValue> image_;
  Member<const CSSNumericLiteralValue> resolution_;
  Member<const CSSImageSetTypeValue> type_;
};

template <>
struct DowncastTraits<CSSImageSetOptionValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsImageSetOptionValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_IMAGE_SET_OPTION_VALUE_H_
