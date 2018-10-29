// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class AppliedTextDecoration {
  DISALLOW_NEW();

 public:
  AppliedTextDecoration(TextDecoration, ETextDecorationStyle, Color);

  TextDecoration Lines() const { return static_cast<TextDecoration>(lines_); }
  ETextDecorationStyle Style() const {
    return static_cast<ETextDecorationStyle>(style_);
  }
  Color GetColor() const { return color_; }
  void SetColor(Color color) { color_ = color; }

  bool operator==(const AppliedTextDecoration&) const;
  bool operator!=(const AppliedTextDecoration& o) const {
    return !(*this == o);
  }

 private:
  unsigned lines_ : kTextDecorationBits;
  unsigned style_ : 3;  // ETextDecorationStyle
  Color color_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_H_
