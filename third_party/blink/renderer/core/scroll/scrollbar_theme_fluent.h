// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_FLUENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_FLUENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_aura.h"

namespace blink {

// The scrollbar theme is only used for Fluent Scrollbars on Windows.
// Please see the visual spec and the design document for more details:
// https://docs.google.com/document/d/1EpJnWAcPCxBQo6zPGR1Tg1NACiIJ-6dk7cYyK1DhBWw
class CORE_EXPORT ScrollbarThemeFluent : public ScrollbarThemeAura {
 public:
  ScrollbarThemeFluent(const ScrollbarThemeFluent&) = delete;
  ScrollbarThemeFluent& operator=(const ScrollbarThemeFluent&) = delete;

  static ScrollbarThemeFluent& GetInstance();
  ~ScrollbarThemeFluent() override = default;

  int ScrollbarThickness(float scale_from_dip,
                         EScrollbarWidth scrollbar_width) override;
  bool UsesOverlayScrollbars() const override;

 protected:
  ScrollbarThemeFluent();

  gfx::Rect ThumbRect(const Scrollbar&) override;
  gfx::Size ButtonSize(const Scrollbar&) const override;

  void PaintTrack(GraphicsContext&,
                  const Scrollbar&,
                  const gfx::Rect&) override;
  void PaintButton(GraphicsContext& context,
                   const Scrollbar& scrollbar,
                   const gfx::Rect& rect,
                   ScrollbarPart part) override;

 private:
  friend class ScrollbarThemeFluentMock;
  int ThumbThickness(const float scale_from_dip) const;

  // Overlay scrollbar tracks have a invisible length-wise inset to give them a
  // floating appearance.
  gfx::Rect InsetButtonRect(const Scrollbar& scrollbar,
                            gfx::Rect rect,
                            ScrollbarPart part);
  gfx::Rect InsetTrackRect(const Scrollbar& scrollbar, gfx::Rect rect);
  int ScrollbarTrackInsetPx(float scale);

  // Button's height for vertical and width for the horizontal scrollbar.
  int scrollbar_button_length_;
  int scrollbar_thumb_thickness_;
  int scrollbar_track_thickness_;

  // Overlay scrollbar-related variables.
  int scrollbar_track_inset_ = 0;
  bool is_fluent_overlay_scrollbar_enabled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_FLUENT_H_
