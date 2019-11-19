/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_THEME_ENGINE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_THEME_ENGINE_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/preferred_color_scheme.h"
#include "third_party/blink/public/platform/web_color_scheme.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_scrollbar_overlay_color_theme.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class PaintCanvas;
}

namespace blink {

class WebThemeEngine {
 public:
  // The current state of the associated Part.
  enum State {
    kStateDisabled,
    kStateHover,
    kStateNormal,
    kStatePressed,
    kStateFocused,
    kStateReadonly,
  };

  // The UI part which is being accessed.
  enum Part {
    // ScrollbarTheme parts
    kPartScrollbarDownArrow,
    kPartScrollbarLeftArrow,
    kPartScrollbarRightArrow,
    kPartScrollbarUpArrow,
    kPartScrollbarHorizontalThumb,
    kPartScrollbarVerticalThumb,
    kPartScrollbarHorizontalTrack,
    kPartScrollbarVerticalTrack,
    kPartScrollbarCorner,

    // LayoutTheme parts
    kPartCheckbox,
    kPartRadio,
    kPartButton,
    kPartTextField,
    kPartMenuList,
    kPartSliderTrack,
    kPartSliderThumb,
    kPartInnerSpinButton,
    kPartProgressBar
  };

  enum class SystemThemeColor {
    kNotSupported,
    kButtonFace,
    kButtonText,
    kGrayText,
    kHighlight,
    kHighlightText,
    kHotlight,
    kMenuHighlight,
    kScrollbar,
    kWindow,
    kWindowText,
    kMaxValue = kWindowText,
  };

  // Extra parameters for drawing the PartScrollbarHorizontalTrack and
  // PartScrollbarVerticalTrack.
  struct ScrollbarTrackExtraParams {
    bool is_back;  // Whether this is the 'back' part or the 'forward' part.

    // The bounds of the entire track, as opposed to the part being painted.
    int track_x;
    int track_y;
    int track_width;
    int track_height;
  };

  // Extra parameters for PartCheckbox, PartPushButton and PartRadio.
  struct ButtonExtraParams {
    bool checked;
    bool indeterminate;  // Whether the button state is indeterminate.
    bool has_border;
    SkColor background_color;
    float zoom;
  };

  // Extra parameters for PartTextField
  struct TextFieldExtraParams {
    bool is_text_area;
    bool is_listbox;
    SkColor background_color;
  };

  // Extra parameters for PartMenuList
  struct MenuListExtraParams {
    bool has_border;
    bool has_border_radius;
    int arrow_x;
    int arrow_y;
    int arrow_size;
    SkColor arrow_color;
    SkColor background_color;
    bool fill_content_area;
  };

  // Extra parameters for PartSliderTrack and PartSliderThumb
  struct SliderExtraParams {
    bool vertical;
    bool in_drag;
    int thumb_x;
    int thumb_y;
    float zoom;
  };

  // Extra parameters for PartInnerSpinButton
  struct InnerSpinButtonExtraParams {
    bool spin_up;
    bool read_only;
  };

  // Extra parameters for PartProgressBar
  struct ProgressBarExtraParams {
    bool determinate;
    int value_rect_x;
    int value_rect_y;
    int value_rect_width;
    int value_rect_height;
  };

  // Extra parameters for scrollbar thumb. Used only for overlay scrollbars.
  struct ScrollbarThumbExtraParams {
    WebScrollbarOverlayColorTheme scrollbar_theme;
  };

  struct ScrollbarButtonExtraParams {
    float zoom;
    bool right_to_left;
  };

  union ExtraParams {
    ScrollbarTrackExtraParams scrollbar_track;
    ButtonExtraParams button;
    TextFieldExtraParams text_field;
    MenuListExtraParams menu_list;
    SliderExtraParams slider;
    InnerSpinButtonExtraParams inner_spin;
    ProgressBarExtraParams progress_bar;
    ScrollbarThumbExtraParams scrollbar_thumb;
    ScrollbarButtonExtraParams scrollbar_button;
  };

  virtual ~WebThemeEngine() {}

  // Gets the size of the given theme part. For variable sized items
  // like vertical scrollbar thumbs, the width will be the required width of
  // the track while the height will be the minimum height.
  virtual WebSize GetSize(Part) { return WebSize(); }

  virtual bool SupportsNinePatch(Part) const { return false; }
  virtual WebSize NinePatchCanvasSize(Part) const { return WebSize(); }
  virtual WebRect NinePatchAperture(Part) const { return WebRect(); }

  struct ScrollbarStyle {
    int thumb_thickness;
    int scrollbar_margin;
    SkColor color;
    base::TimeDelta fade_out_delay;
    base::TimeDelta fade_out_duration;
  };

  // Gets the overlay scrollbar style. Not used on Mac.
  virtual void GetOverlayScrollbarStyle(ScrollbarStyle* style) {
    // Disable overlay scrollbar fade out (for non-composited scrollers) unless
    // explicitly enabled by the implementing child class. NOTE: these values
    // aren't used to control Mac fade out - that happens in ScrollAnimatorMac.
    style->fade_out_delay = base::TimeDelta();
    style->fade_out_duration = base::TimeDelta();
    // The other fields in this struct are used only on Android to draw solid
    // color scrollbars. On other platforms the scrollbars are painted in
    // NativeTheme so these fields are unused in non-Android WebThemeEngines.
  }

  // Paint the given the given theme part.
  virtual void Paint(cc::PaintCanvas*,
                     Part,
                     State,
                     const WebRect&,
                     const ExtraParams*,
                     blink::WebColorScheme) {}

  virtual base::Optional<SkColor> GetSystemColor(
      SystemThemeColor system_theme) const {
    return base::nullopt;
  }

  virtual ForcedColors GetForcedColors() const { return ForcedColors::kNone; }
  virtual void SetForcedColors(const blink::ForcedColors forced_colors) {}
  virtual blink::PreferredColorScheme PreferredColorScheme() const {
    return PreferredColorScheme::kNoPreference;
  }
  virtual void SetPreferredColorScheme(
      const blink::PreferredColorScheme preferred_color_scheme) {}
};

}  // namespace blink

#endif
