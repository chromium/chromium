// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_EDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_EDGE_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct BorderEdge {
  STACK_ALLOCATED();

 public:
  BorderEdge(float edge_width,
             const Color& edge_color,
             EBorderStyle edge_style,
             bool edge_is_present = true);
  BorderEdge();

  bool HasVisibleColorAndStyle() const;
  bool ShouldRender() const;
  bool PresentButInvisible() const;
  bool ObscuresBackgroundEdge() const;
  bool ObscuresBackground() const;
  float UsedWidth() const;

  bool SharesColorWith(const BorderEdge& other) const;

  EBorderStyle BorderStyle() const { return static_cast<EBorderStyle>(style); }

  enum DoubleBorderStripe {
    kDoubleBorderStripeOuter,
    kDoubleBorderStripeInner
  };

  float GetDoubleBorderStripeWidth(DoubleBorderStripe) const;

  float Width() const { return width_; }

  void ClampWidth(float width) {
    if (width_ > width)
      width_ = width;
  }

  Color color;
  bool is_present;

 private:
  unsigned style : 4;  // EBorderStyle
  float width_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_BORDER_EDGE_H_
