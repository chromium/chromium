// Copyright 2022 The Chromium Authors. All rights reserved.
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

 protected:
  ScrollbarThemeFluent() = default;

  gfx::Rect ThumbRect(const Scrollbar&) override;
  gfx::Size ButtonSize(const Scrollbar&) const override;

 private:
  int ThumbThickness(const float scale_from_dip) const;

  // TODO(crbug.com/1353042): Remove hardcoded values. Get dimensions from
  // ui/native_theme via WebThemeEngine.
  // Button height for vertical scrollbar and width for horizontal.
  int scrollbar_button_length_ = 18;
  int scrollbar_thumb_thickness_ = 6;
  int scrollbar_track_thickness_ = 14;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_FLUENT_H_
