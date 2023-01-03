// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"

namespace blink {

class CSSContainerValues : public MediaValuesDynamic {
 public:
  explicit CSSContainerValues(Document& document,
                              Element& container,
                              absl::optional<double> width,
                              absl::optional<double> height);

  // Returns absl::nullopt if queries on the relevant axis is not
  // supported.
  absl::optional<double> Width() const override { return width_; }
  absl::optional<double> Height() const override { return height_; }

  void Trace(Visitor*) const override;

 protected:
  float EmFontSize(float zoom) const override;
  float RemFontSize(float zoom) const override;
  float ExFontSize(float zoom) const override;
  float RexFontSize(float zoom) const override;
  float ChFontSize(float zoom) const override;
  float RchFontSize(float zoom) const override;
  float IcFontSize(float zoom) const override;
  float RicFontSize(float zoom) const override;
  float LineHeight(float zoom) const override;
  float RootLineHeight(float zoom) const override;
  // Note that ContainerWidth/ContainerHeight are used to resolve
  // container *units*. See `container_sizes_`.
  Element* ContainerElement() const override { return element_; }
  double ContainerWidth() const override;
  double ContainerHeight() const override;
  WritingMode GetWritingMode() const override { return writing_mode_; }

 private:
  // The current computed style for the container.
  Member<Element> element_;
  // Container width in CSS pixels.
  absl::optional<double> width_;
  // Container height in CSS pixels.
  absl::optional<double> height_;
  // The writing-mode of the container.
  WritingMode writing_mode_;
  // Container font sizes for resolving relative lengths.
  CSSToLengthConversionData::FontSizes font_sizes_;
  // LineHeightSize of the container element.
  CSSToLengthConversionData::LineHeightSize line_height_size_;
  // Used to resolve container-relative units found in the @container prelude.
  // Such units refer to container sizes of *ancestor* containers, and must
  // not be confused with the size of the *current* container (which is stored
  // in `width_` and `height_`).
  CSSToLengthConversionData::ContainerSizes container_sizes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_
