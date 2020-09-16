/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CUSTOM_SCROLLBAR_THEME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CUSTOM_SCROLLBAR_THEME_H_

#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"

namespace blink {

class LayoutCustomScrollbarPart;
class WebMouseEvent;
struct PhysicalRect;

class CustomScrollbarTheme final : public ScrollbarTheme {
 public:
  ~CustomScrollbarTheme() override = default;

  int ScrollbarThickness(float scale_from_dip) override {
    return GetTheme().ScrollbarThickness(scale_from_dip);
  }

  bool NativeThemeHasButtons() override {
    return GetTheme().NativeThemeHasButtons();
  }

  void PaintScrollCorner(GraphicsContext&,
                         const Scrollbar* vertical_scrollbar,
                         const DisplayItemClient&,
                         const IntRect& corner_rect,
                         ColorScheme color_scheme) override;

  bool ShouldCenterOnThumb(const Scrollbar& scrollbar,
                           const WebMouseEvent& event) override {
    return GetTheme().ShouldCenterOnThumb(scrollbar, event);
  }
  bool ShouldSnapBackToDragOrigin(const Scrollbar& scrollbar,
                                  const WebMouseEvent& event) override {
    return GetTheme().ShouldSnapBackToDragOrigin(scrollbar, event);
  }

  base::TimeDelta InitialAutoscrollTimerDelay() override {
    return GetTheme().InitialAutoscrollTimerDelay();
  }
  base::TimeDelta AutoscrollTimerDelay() override {
    return GetTheme().AutoscrollTimerDelay();
  }

  void RegisterScrollbar(Scrollbar& scrollbar) override {
    return GetTheme().RegisterScrollbar(scrollbar);
  }

  int MinimumThumbLength(const Scrollbar&) override;

  void ButtonSizesAlongTrackAxis(const Scrollbar&,
                                 int& before_size,
                                 int& after_size);

  static CustomScrollbarTheme* GetCustomScrollbarTheme();

  static void PaintIntoRect(const LayoutCustomScrollbarPart&,
                            GraphicsContext&,
                            const PhysicalRect&);

 protected:
  ScrollbarPart HitTest(const Scrollbar&, const IntPoint&) override;

  bool HasButtons(const Scrollbar&) override;
  bool HasThumb(const Scrollbar&) override;

  IntRect BackButtonRect(const Scrollbar&) override;
  IntRect ForwardButtonRect(const Scrollbar&) override;
  IntRect TrackRect(const Scrollbar&) override;

  void PaintTrackAndButtons(GraphicsContext&,
                            const Scrollbar&,
                            const IntPoint&) override;
  void PaintButton(GraphicsContext&,
                   const Scrollbar&,
                   const IntRect&,
                   ScrollbarPart) override;
  void PaintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) override;
  void PaintTickmarks(GraphicsContext&,
                      const Scrollbar&,
                      const IntRect&) override;

  IntRect ConstrainTrackRectToTrackPieces(const Scrollbar&,
                                          const IntRect&) override;

 private:
  IntRect ButtonRect(const Scrollbar&, ScrollbarPart);
  void PaintScrollbarBackground(GraphicsContext&, const Scrollbar&);
  void PaintTrackBackground(GraphicsContext&, const Scrollbar&, const IntRect&);
  void PaintTrackPiece(GraphicsContext&,
                       const Scrollbar&,
                       const IntRect&,
                       ScrollbarPart);
  void PaintPart(GraphicsContext&,
                 const Scrollbar&,
                 const IntRect&,
                 ScrollbarPart);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CUSTOM_SCROLLBAR_THEME_H_
