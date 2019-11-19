// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_OVERLAY_MOBILE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_OVERLAY_MOBILE_H_

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class CORE_EXPORT ScrollbarThemeOverlayMobile : public ScrollbarThemeOverlay {
 public:
  static ScrollbarThemeOverlayMobile& GetInstance();

  void PaintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) override;
  bool AllowsHitTest() const override { return false; }
  ScrollbarPart HitTest(const Scrollbar&, const IntPoint&) override {
    return kNoPart;
  }
  bool IsSolidColor() const override { return true; }
  bool UsesNinePatchThumbResource() const override { return false; }

 protected:
  ScrollbarThemeOverlayMobile(int thumb_thickness, int scrollbar_margin, Color);

 private:
  Color color_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_OVERLAY_MOBILE_H_
