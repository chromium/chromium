// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_VALUES_H_

#include <optional>

#include "third_party/blink/renderer/core/css/container_state.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"

namespace blink {

class CORE_EXPORT CSSContainerValues : public MediaValuesDynamic {
 public:
  explicit CSSContainerValues(
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
      const PositionTryFallback& fallback);

  // Returns std::nullopt if queries on the relevant axis is not
  // supported.
  std::optional<double> Width() const override { return width_; }
  std::optional<double> Height() const override { return height_; }

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
  float CapFontSize(float zoom) const override;
  float RcapFontSize(float zoom) const override;
  Element* GetElement() const override { return element_.Get(); }
  // Note that ContainerWidth/ContainerHeight are used to resolve
  // container *units*. See `container_sizes_`.
  Element* ContainerElement() const override { return element_.Get(); }
  double ContainerWidth() const override;
  double ContainerHeight() const override;
  WritingMode GetWritingMode() const override {
    return writing_direction_.GetWritingMode();
  }
  WritingDirectionMode GetWritingDirection() const override {
    return writing_direction_;
  }
  ContainerStuckPhysical StuckHorizontal() const override {
    return stuck_horizontal_;
  }
  ContainerStuckPhysical StuckVertical() const override {
    return stuck_vertical_;
  }
  ContainerStuckLogical StuckInline() const override;
  ContainerStuckLogical StuckBlock() const override;
  ContainerSnappedFlags SnappedFlags() const override { return snapped_; }
  ContainerScrollableFlags ScrollableHorizontal() const override {
    return scrollable_horizontal_;
  }
  ContainerScrollableFlags ScrollableVertical() const override {
    return scrollable_vertical_;
  }
  ContainerScrollableFlags ScrollableInline() const override;
  ContainerScrollableFlags ScrollableBlock() const override;
  ContainerScrolled ScrolledHorizontal() const override {
    return scrolled_horizontal_;
  }
  ContainerScrolled ScrolledVertical() const override {
    return scrolled_vertical_;
  }
  ContainerScrolled ScrolledInline() const override;
  ContainerScrolled ScrolledBlock() const override;

  WritingDirectionMode AbsContainerWritingDirection() const override {
    return abs_container_writing_direction_;
  }
  const PositionTryFallback& AnchoredFallback() const override {
    return anchored_fallback_;
  }

 private:
  // The current computed style for the container.
  Member<Element> element_;
  // Container width in CSS pixels.
  std::optional<double> width_;
  // Container height in CSS pixels.
  std::optional<double> height_;
  // The writing-mode and direction of the container.
  WritingDirectionMode writing_direction_;
  // The writing-mode and direction of the absolute positioned containing block
  // for an anchored container.
  WritingDirectionMode abs_container_writing_direction_;
  // Whether a sticky container is horizontally stuck and to which edge.
  ContainerStuckPhysical stuck_horizontal_ = ContainerStuckPhysical::kNo;
  // Whether a sticky container is vertically stuck and against which edge.
  ContainerStuckPhysical stuck_vertical_ = ContainerStuckPhysical::kNo;
  // Union of flags for whether a scroll-snapped container is snapped in block
  // or inline directions.
  // TODO(crbug.com/1475231): Need to update this from the scroll snapshot.
  ContainerSnappedFlags snapped_ =
      static_cast<ContainerSnappedFlags>(ContainerSnapped::kNone);
  // Whether a scroll-state container has horizontally scrollable overflow.
  ContainerScrollableFlags scrollable_horizontal_ =
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone);
  // Whether a scroll-state container has vertically scrollable overflow.
  ContainerScrollableFlags scrollable_vertical_ =
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone);
  ContainerScrolled scrolled_horizontal_ = ContainerScrolled::kNone;
  ContainerScrolled scrolled_vertical_ = ContainerScrolled::kNone;
  // The option from position-try-fallbacks applied to anchored() container.
  // If no fallback is applied, PositionTryFallback::IsNone() returns true.
  PositionTryFallback anchored_fallback_;
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
