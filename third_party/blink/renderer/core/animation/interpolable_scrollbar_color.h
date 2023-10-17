// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SCROLLBAR_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SCROLLBAR_COLOR_H_

#include <memory>
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/interpolable_color.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/style_scrollbar_color.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class StyleResolverState;

class CORE_EXPORT InterpolableScrollbarColor : public InterpolableValue {
 public:
  InterpolableScrollbarColor();

  static std::unique_ptr<InterpolableScrollbarColor> Create(
      StyleScrollbarColor);
  bool IsScrollbarColor() const final { return true; }

  StyleScrollbarColor GetScrollbarColor(const StyleResolverState&) const;

  void Scale(double scale) final;
  void Add(const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }

  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;

  std::unique_ptr<InterpolableScrollbarColor> Clone() const {
    return std::unique_ptr<InterpolableScrollbarColor>(RawClone());
  }

  void Composite(const InterpolableScrollbarColor& other, double fraction);

 private:
  InterpolableScrollbarColor(InterpolableColor thumb_color,
                             InterpolableColor track_color);

  InterpolableScrollbarColor* RawClone() const final;
  InterpolableScrollbarColor* RawCloneAndZero() const final;

  InterpolableColor thumb_color_;
  InterpolableColor track_color_;
};

template <>
struct DowncastTraits<InterpolableScrollbarColor> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsScrollbarColor();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_SCROLLBAR_COLOR_H_
