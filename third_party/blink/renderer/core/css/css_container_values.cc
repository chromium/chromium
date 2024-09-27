// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"

namespace blink {

CSSContainerValues::CSSContainerValues(Document& document,
                                       Element& container,
                                       std::optional<double> width,
                                       std::optional<double> height,
                                       ContainerStuckPhysical stuck_horizontal,
                                       ContainerStuckPhysical stuck_vertical,
                                       ContainerSnappedFlags snapped)
    : MediaValuesDynamic(document.GetFrame()),
      element_(&container),
      width_(width),
      height_(height),
      writing_direction_(container.ComputedStyleRef().GetWritingDirection()),
      stuck_horizontal_(stuck_horizontal),
      stuck_vertical_(stuck_vertical),
      snapped_(snapped),
      font_sizes_(CSSToLengthConversionData::FontSizes(
          container.ComputedStyleRef().GetFontSizeStyle(),
          document.documentElement()->GetComputedStyle())),
      line_height_size_(CSSToLengthConversionData::LineHeightSize(
          container.ComputedStyleRef().GetFontSizeStyle(),
          document.documentElement()->GetComputedStyle())),
      font_style_(container.GetComputedStyle()),
      root_font_style_(document.documentElement()->GetComputedStyle()),
      container_sizes_(
          ContainerQueryEvaluator::ParentContainerCandidateElement(container)) {
}

void CSSContainerValues::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(container_sizes_);
  visitor->Trace(font_style_);
  visitor->Trace(root_font_style_);
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

float CSSContainerValues::RexFontSize(float zoom) const {
  return font_sizes_.Rex(zoom);
}

float CSSContainerValues::ChFontSize(float zoom) const {
  return font_sizes_.Ch(zoom);
}

float CSSContainerValues::RchFontSize(float zoom) const {
  return font_sizes_.Rch(zoom);
}

float CSSContainerValues::IcFontSize(float zoom) const {
  return font_sizes_.Ic(zoom);
}

float CSSContainerValues::RicFontSize(float zoom) const {
  return font_sizes_.Ric(zoom);
}

float CSSContainerValues::LineHeight(float zoom) const {
  return line_height_size_.Lh(zoom);
}

float CSSContainerValues::RootLineHeight(float zoom) const {
  return line_height_size_.Rlh(zoom);
}

float CSSContainerValues::CapFontSize(float zoom) const {
  return font_sizes_.Cap(zoom);
}

float CSSContainerValues::RcapFontSize(float zoom) const {
  return font_sizes_.Rcap(zoom);
}

double CSSContainerValues::ContainerWidth() const {
  return container_sizes_.Width().value_or(SmallViewportWidth());
}

double CSSContainerValues::ContainerHeight() const {
  return container_sizes_.Height().value_or(SmallViewportHeight());
}

namespace {

// Converts from left/right/top/bottom to start/end as if the writing mode and
// direction was horizontal-tb and ltr.
ContainerStuckLogical PhysicalToLogicalLtrHorizontalTb(
    ContainerStuckPhysical physical) {
  switch (physical) {
    case ContainerStuckPhysical::kNo:
      return ContainerStuckLogical::kNo;
    case ContainerStuckPhysical::kLeft:
    case ContainerStuckPhysical::kTop:
      return ContainerStuckLogical::kStart;
    case ContainerStuckPhysical::kRight:
    case ContainerStuckPhysical::kBottom:
      return ContainerStuckLogical::kEnd;
  }
}

}  // namespace

ContainerStuckLogical CSSContainerValues::StuckInline() const {
  // TODO(crbug.com/1445189): The WritingDirection should be taken from the
  // container's containing block, not the container. Otherwise the inset
  // properties on the sticky positioned will not match the same inset features
  // in container queries when writing-mode or direction changes on the sticky
  // positioned itself.
  ContainerStuckPhysical physical =
      writing_direction_.IsHorizontal() ? StuckHorizontal() : StuckVertical();
  ContainerStuckLogical logical = PhysicalToLogicalLtrHorizontalTb(physical);
  return writing_direction_.IsRtl() ? Flip(logical) : logical;
}

ContainerStuckLogical CSSContainerValues::StuckBlock() const {
  // TODO(crbug.com/1445189): The WritingDirection should be taken from the
  // container's containing block, not the container. Otherwise the inset
  // properties on the sticky positioned will not match the same inset features
  // in container queries when writing-mode or direction changes on the sticky
  // positioned itself.
  ContainerStuckPhysical physical =
      writing_direction_.IsHorizontal() ? StuckVertical() : StuckHorizontal();
  ContainerStuckLogical logical = PhysicalToLogicalLtrHorizontalTb(physical);
  return writing_direction_.IsFlippedBlocks() ? Flip(logical) : logical;
}

}  // namespace blink
