// Copyright 2021 The Chromium Authors. All rights reserved.
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
      style_(container.GetComputedStyle()),
      width_(width),
      height_(height),
      writing_mode_(container.ComputedStyleRef().GetWritingMode()),
      font_sizes_(CSSToLengthConversionData::FontSizes(
                      container.GetComputedStyle(),
                      document.documentElement()->GetComputedStyle())
                      .Unzoomed()),
      container_sizes_(container.ParentOrShadowHostElement()) {}

void CSSContainerValues::Trace(Visitor* visitor) const {
  visitor->Trace(container_sizes_);
  MediaValuesDynamic::Trace(visitor);
}

float CSSContainerValues::EmFontSize() const {
  return font_sizes_.Em();
}

float CSSContainerValues::RemFontSize() const {
  return font_sizes_.Rem();
}

float CSSContainerValues::ExFontSize() const {
  return font_sizes_.Ex();
}

float CSSContainerValues::ChFontSize() const {
  return font_sizes_.Ch();
}

double CSSContainerValues::ContainerWidth() const {
  return container_sizes_.Width().value_or(SmallViewportWidth());
}

double CSSContainerValues::ContainerHeight() const {
  return container_sizes_.Height().value_or(SmallViewportHeight());
}

}  // namespace blink
