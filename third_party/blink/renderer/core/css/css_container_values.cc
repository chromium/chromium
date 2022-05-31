// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"

namespace blink {

CSSContainerValues::CSSContainerValues(Document& document,
                                       const ComputedStyle& style,
                                       absl::optional<double> width,
                                       absl::optional<double> height)
    : MediaValuesDynamic(document.GetFrame()),
      width_(width),
      height_(height),
      writing_mode_(style.GetWritingMode()),
      font_sizes_(CSSToLengthConversionData::FontSizes(
                      &style,
                      document.documentElement()->GetComputedStyle())
                      .Unzoomed()) {}

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

}  // namespace blink
