// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_values.h"

#include "third_party/blink/renderer/core/css/container_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/position_try_fallbacks.h"

namespace blink {

CSSContainerValues::CSSContainerValues(
    Document& document,
    Element& container,
    std::optional<double> width,
    std::optional<double> height,
    ContainerStuckPhysical stuck_horizontal,
    ContainerStuckPhysical stuck_vertical,
    ContainerSnappedFlags snapped,
    ContainerScrollableFlags scrollable_horizontal,
    ContainerScrollableFlags scrollable_vertical,
    ContainerScrolled scrolled_horizontal,
    ContainerScrolled scrolled_vertical,
    WritingDirectionMode abs_container_writing_direction,
    const PositionTryFallback& anchored_fallback)
    : MediaValuesDynamic(document.GetFrame()),
      element_(&container),
      width_(width),
      height_(height),
      writing_direction_(container.ComputedStyleRef().GetWritingDirection()),
      abs_container_writing_direction_(abs_container_writing_direction),
      stuck_horizontal_(stuck_horizontal),
      stuck_vertical_(stuck_vertical),
      snapped_(snapped),
      scrollable_horizontal_(scrollable_horizontal),
      scrollable_vertical_(scrollable_vertical),
      scrolled_horizontal_(scrolled_horizontal),
      scrolled_vertical_(scrolled_vertical),
      anchored_fallback_(anchored_fallback),
      font_sizes_(CSSToLengthConversionData::FontSizes(
          container.ComputedStyleRef().GetFontSizeStyle(),
          document.documentElement()->GetComputedStyle())),
      line_height_size_(CSSToLengthConversionData::LineHeightSize(
          container.ComputedStyleRef().GetFontSizeStyle(),
          document.documentElement()->GetComputedStyle())),
      container_sizes_(FlatTreeTraversal::ParentElement(container)) {}

void CSSContainerValues::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(container_sizes_);
  visitor->Trace(font_sizes_);
  visitor->Trace(line_height_size_);
  visitor->Trace(anchored_fallback_);
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
  ContainerStuckPhysical physical =
      writing_direction_.IsHorizontal() ? StuckHorizontal() : StuckVertical();
  ContainerStuckLogical logical = PhysicalToLogicalLtrHorizontalTb(physical);
  return writing_direction_.IsRtl() ? Flip(logical) : logical;
}

ContainerStuckLogical CSSContainerValues::StuckBlock() const {
  ContainerStuckPhysical physical =
      writing_direction_.IsHorizontal() ? StuckVertical() : StuckHorizontal();
  ContainerStuckLogical logical = PhysicalToLogicalLtrHorizontalTb(physical);
  return writing_direction_.IsFlippedBlocks() ? Flip(logical) : logical;
}

ContainerScrollableFlags CSSContainerValues::ScrollableInline() const {
  ContainerScrollableFlags scrollable_inline = writing_direction_.IsHorizontal()
                                                   ? ScrollableHorizontal()
                                                   : ScrollableVertical();
  return writing_direction_.IsRtl() ? Flip(scrollable_inline)
                                    : scrollable_inline;
}

ContainerScrollableFlags CSSContainerValues::ScrollableBlock() const {
  ContainerScrollableFlags scrollable_block = writing_direction_.IsHorizontal()
                                                  ? ScrollableVertical()
                                                  : ScrollableHorizontal();
  return writing_direction_.IsFlippedBlocks() ? Flip(scrollable_block)
                                              : scrollable_block;
}

ContainerScrolled CSSContainerValues::ScrolledInline() const {
  ContainerScrolled scrolled_inline = writing_direction_.IsHorizontal()
                                          ? ScrolledHorizontal()
                                          : ScrolledVertical();
  return writing_direction_.IsRtl() ? Flip(scrolled_inline) : scrolled_inline;
}

ContainerScrolled CSSContainerValues::ScrolledBlock() const {
  ContainerScrolled scrolled_block = writing_direction_.IsHorizontal()
                                         ? ScrolledVertical()
                                         : ScrolledHorizontal();
  return writing_direction_.IsFlippedBlocks() ? Flip(scrolled_block)
                                              : scrolled_block;
}

}  // namespace blink
