// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"

namespace blink {

CSSContainerValues::CSSContainerValues(Document& document,
                                       Element& container,
                                       absl::optional<double> width,
                                       absl::optional<double> height)
    : MediaValuesDynamic(document.GetFrame()),
      element_(&container),
      width_(width),
      height_(height),
      writing_mode_(container.ComputedStyleRef().GetWritingMode()),
      font_sizes_(CSSToLengthConversionData::FontSizes(
          container.GetComputedStyle(),
          document.documentElement()->GetComputedStyle())),
      line_height_size_(CSSToLengthConversionData::LineHeightSize(
          container.ComputedStyleRef())),
      container_sizes_(container.ParentOrShadowHostElement()) {}

void CSSContainerValues::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(container_sizes_);
  MediaValuesDynamic::Trace(visitor);
}

float CSSContainerValues::EmFontSize(float zoom) const {
  return font_sizes_.Em(zoom);
}

float CSSContainerValues::RemFontSize(float zoom) const {
  return font_sizes_.Rem(zoom);
}

float CSSContainerValues::ExFontSize(float zoom) const {
  return font_sizes_.Ex(zoom);
}

float CSSContainerValues::ChFontSize(float zoom) const {
  return font_sizes_.Ch(zoom);
}

float CSSContainerValues::IcFontSize(float zoom) const {
  return font_sizes_.Ic(zoom);
}

float CSSContainerValues::LineHeight(float zoom) const {
  return line_height_size_.Lh(zoom);
}

double CSSContainerValues::ContainerWidth() const {
  return container_sizes_.Width().value_or(SmallViewportWidth());
}

double CSSContainerValues::ContainerHeight() const {
  return container_sizes_.Height().value_or(SmallViewportHeight());
}

}  // namespace blink
