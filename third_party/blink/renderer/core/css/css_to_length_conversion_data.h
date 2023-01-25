/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TO_LENGTH_CONVERSION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TO_LENGTH_CONVERSION_DATA_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class ComputedStyle;
class LayoutView;
class Font;
class Element;

class CORE_EXPORT CSSToLengthConversionData : public CSSLengthResolver {
  STACK_ALLOCATED();

 public:
  class CORE_EXPORT FontSizes {
    DISALLOW_NEW();

   public:
    FontSizes() = default;
    FontSizes(float em, float rem, const Font*, float font_zoom);
    FontSizes(float em,
              float rem,
              const Font* font,
              const Font* root_font,
              float font_zoom,
              float root_font_zoom);
    FontSizes(const ComputedStyle*, const ComputedStyle* root_style);

    float Em(float zoom) const { return em_ * zoom; }
    float Rem(float zoom) const { return rem_ * zoom; }
    float Ex(float zoom) const;
    float Rex(float zoom) const;
    float Ch(float zoom) const;
    float Rch(float zoom) const;
    float Ic(float zoom) const;
    float Ric(float zoom) const;

   private:
    float em_ = 0;
    float rem_ = 0;
    const Font* font_ = nullptr;
    const Font* root_font_ = nullptr;
    // Font-metrics-based units (ex, ch, ic) are pre-zoomed by a factor of
    // `font_zoom_`.
    float font_zoom_ = 1;
    float root_font_zoom_ = 1;
  };

  class CORE_EXPORT LineHeightSize {
    DISALLOW_NEW();

   public:
    LineHeightSize() = default;
    LineHeightSize(const Length& line_height, const Font* font, float font_zoom)
        : line_height_(line_height), font_(font), font_zoom_(font_zoom) {}
    LineHeightSize(const Length& line_height,
                   const Length& root_line_height,
                   const Font* font,
                   const Font* root_font,
                   float font_zoom,
                   float root_font_zoom)
        : line_height_(line_height),
          root_line_height_(root_line_height),
          font_(font),
          root_font_(root_font),
          font_zoom_(font_zoom),
          root_font_zoom_(root_font_zoom) {}
    LineHeightSize(const ComputedStyle& style, const ComputedStyle* root_style);

    float Lh(float zoom) const;
    float Rlh(float zoom) const;

   private:
    Length line_height_;
    Length root_line_height_;
    // Note that this Font may be different from the instance held
    // by FontSizes (for the same CSSToLengthConversionData object).
    const Font* font_ = nullptr;
    const Font* root_font_ = nullptr;
    // Like ex/ch/ic, lh is also based on font-metrics and is pre-zoomed by
    // a factor of `font_zoom_`.
    float font_zoom_ = 1;
    float root_font_zoom_ = 1;
  };

  class CORE_EXPORT ViewportSize {
    DISALLOW_NEW();

   public:
    ViewportSize() = default;
    ViewportSize(double width, double height)
        : large_width_(width),
          large_height_(height),
          small_width_(width),
          small_height_(height),
          dynamic_width_(width),
          dynamic_height_(height) {}

    explicit ViewportSize(const LayoutView*);

    // v*
    double Width() const { return LargeWidth(); }
    double Height() const { return LargeHeight(); }

    // lv*
    double LargeWidth() const { return large_width_; }
    double LargeHeight() const { return large_height_; }

    // sv*
    double SmallWidth() const { return small_width_; }
    double SmallHeight() const { return small_height_; }

    // dv*
    double DynamicWidth() const { return dynamic_width_; }
    double DynamicHeight() const { return dynamic_height_; }

   private:
    // v*, lv*
    double large_width_ = 0;
    double large_height_ = 0;
    // sv*
    double small_width_ = 0;
    double small_height_ = 0;
    // dv*
    double dynamic_width_ = 0;
    double dynamic_height_ = 0;
  };

  class CORE_EXPORT ContainerSizes {
    DISALLOW_NEW();

   public:
    ContainerSizes() = default;

    // ContainerSizes will look for container-query containers in the inclusive
    // ancestor chain of `context_element`. Optimally, the nearest container-
    // query container is provided, although it's harmless to provide some
    // descendant of that container (we'll just traverse a bit more).
    explicit ContainerSizes(Element* context_element)
        : context_element_(context_element) {}

    // ContainerSizes::Width/Height is normally computed lazily by looking
    // the ancestor chain of `context_element_`. This function allows the
    // sizes to be fetched eagerly instead. This is useful for situations where
    // we don't have enough context to fetch the information lazily (e.g.
    // generated images).
    ContainerSizes PreCachedCopy() const;

    // Note that this will eagerly compute width/height for both `this` and
    // the incoming object.
    bool SizesEqual(const ContainerSizes&) const;

    void Trace(Visitor*) const;

    absl::optional<double> Width() const;
    absl::optional<double> Height() const;

   private:
    void CacheSizeIfNeeded(PhysicalAxes, absl::optional<double>& cache) const;

    Member<Element> context_element_;
    mutable PhysicalAxes cached_physical_axes_{kPhysicalAxisNone};
    mutable absl::optional<double> cached_width_;
    mutable absl::optional<double> cached_height_;
  };

  using Flags = uint8_t;

  // Flags represent the units seen in a conversion. They are used for targeted
  // invalidation, e.g. when root font-size changes, only elements dependent on
  // rem units are recalculated.
  enum class Flag : Flags {
    // em
    kEm = 1u << 0,
    // rem
    kRootFontRelative = 1u << 1,
    // ex, ch, ic, lh
    kGlyphRelative = 1u << 2,
    // rex, rch, ric have both kRootFontRelative and kGlyphRelative
    // lh
    kLineHeightRelative = 1u << 3,
    // sv*, lv*, v*
    kStaticViewport = 1u << 4,
    // dv*
    kDynamicViewport = 1u << 5,
    // cq*
    kContainerRelative = 1u << 6,
  };

  CSSToLengthConversionData() : CSSLengthResolver(1 /* zoom */) {}
  CSSToLengthConversionData(WritingMode,
                            const FontSizes&,
                            const LineHeightSize&,
                            const ViewportSize&,
                            const ContainerSizes&,
                            float zoom,
                            Flags&);
  CSSToLengthConversionData(const ComputedStyle* element_style,
                            const ComputedStyle* parent_style,
                            const ComputedStyle* root_style,
                            const LayoutView*,
                            const ContainerSizes&,
                            float zoom,
                            Flags&);

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
  double ViewportWidth() const override;
  double ViewportHeight() const override;
  double SmallViewportWidth() const override;
  double SmallViewportHeight() const override;
  double LargeViewportWidth() const override;
  double LargeViewportHeight() const override;
  double DynamicViewportWidth() const override;
  double DynamicViewportHeight() const override;
  double ContainerWidth() const override;
  double ContainerHeight() const override;
  WritingMode GetWritingMode() const override;

  void SetFontSizes(const FontSizes& font_sizes) { font_sizes_ = font_sizes; }
  void SetLineHeightSize(const LineHeightSize& line_height_size) {
    line_height_size_ = line_height_size;
  }

  // See ContainerSizes::PreCachedCopy.
  //
  // Calling this function will mark the associated ComputedStyle as
  // dependent on container-relative units.
  ContainerSizes PreCachedContainerSizesCopy() const;

  CSSToLengthConversionData CopyWithAdjustedZoom(float new_zoom) const {
    DCHECK(flags_);
    return CSSToLengthConversionData(writing_mode_, font_sizes_,
                                     line_height_size_, viewport_size_,
                                     container_sizes_, new_zoom, *flags_);
  }
  CSSToLengthConversionData Unzoomed() const {
    return CopyWithAdjustedZoom(1.0f);
  }

 private:
  void SetFlag(Flag flag) const {
    if (flags_) {
      *flags_ |= static_cast<Flags>(flag);
    }
  }

  WritingMode writing_mode_ = WritingMode::kHorizontalTb;
  FontSizes font_sizes_;
  LineHeightSize line_height_size_;
  ViewportSize viewport_size_;
  ContainerSizes container_sizes_;
  mutable Flags* flags_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TO_LENGTH_CONVERSION_DATA_H_
