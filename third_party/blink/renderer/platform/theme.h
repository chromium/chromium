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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_H_

#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_box.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/theme_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class GraphicsContext;
class ScrollableArea;

// Unlike other platform classes, Theme does extensively use virtual functions.
// This design allows a platform to switch between multiple themes at runtime.
class PLATFORM_EXPORT Theme {
  USING_FAST_MALLOC(Theme);
  WTF_MAKE_NONCOPYABLE(Theme);

 public:
  Theme() = default;
  virtual ~Theme() = default;

  // A method to obtain the baseline position adjustment for a "leaf" control.
  // This will only be used if a baseline position cannot be determined by
  // examining child content. Checkboxes and radio buttons are examples of
  // controls that need to do this.  The adjustment is an offset that adds to
  // the baseline, e.g., marginTop() + height() + |offset|.
  // The offset is not zoomed.
  virtual int BaselinePositionAdjustment(ControlPart) const { return 0; }

  // A method asking if the control changes its appearance when the window is
  // inactive.
  virtual bool ControlHasInactiveAppearance(ControlPart) const { return false; }

  // General methods for whether or not any of the controls in the theme change
  // appearance when the window is inactive or when hovered over.
  virtual bool ControlsCanHaveInactiveAppearance() const { return false; }
  virtual bool ControlsCanHaveHoveredAppearance() const { return false; }

  // Used by LayoutTheme::isControlStyled to figure out if the native look and
  // feel should be turned off.
  virtual bool ControlDrawsBorder(ControlPart) const { return true; }
  virtual bool ControlDrawsBackground(ControlPart) const { return true; }
  virtual bool ControlDrawsFocusOutline(ControlPart) const { return true; }

  // Methods for obtaining platform-specific colors.
  virtual Color SelectionColor(ControlPart, ControlState, SelectionPart) const {
    return Color();
  }
  virtual Color TextSearchHighlightColor() const { return Color(); }

  // CSS system colors and fonts
  virtual Color SystemColor(ThemeColor) const { return Color(); }

  // How fast the caret blinks in text fields.
  virtual TimeDelta CaretBlinkInterval() const {
    return TimeDelta::FromMilliseconds(500);
  }

  // Methods used to adjust the ComputedStyles of controls.

  // The font description result should have a zoomed font size.
  virtual FontDescription ControlFont(ControlPart,
                                      const FontDescription& font_description,
                                      float /*zoomFactor*/) const {
    return font_description;
  }

  // The size here is in zoomed coordinates already.  If a new size is returned,
  // it also needs to be in zoomed coordinates.
  virtual LengthSize GetControlSize(ControlPart,
                                    const FontDescription&,
                                    const LengthSize& zoomed_size,
                                    float /*zoomFactor*/) const {
    return zoomed_size;
  }

  // Returns the minimum size for a control in zoomed coordinates.
  virtual LengthSize MinimumControlSize(ControlPart,
                                        const FontDescription&,
                                        float /*zoomFactor*/) const {
    return LengthSize(Length(0, kFixed), Length(0, kFixed));
  }

  // Allows the theme to modify the existing padding/border.
  virtual LengthBox ControlPadding(ControlPart,
                                   const FontDescription&,
                                   const Length& zoomed_box_top,
                                   const Length& zoomed_box_right,
                                   const Length& zoomed_box_bottom,
                                   const Length& zoomed_box_left,
                                   float zoom_factor) const;
  virtual LengthBox ControlBorder(ControlPart,
                                  const FontDescription&,
                                  const LengthBox& zoomed_box,
                                  float zoom_factor) const;

  // Whether or not whitespace: pre should be forced on always.
  virtual bool ControlRequiresPreWhiteSpace(ControlPart) const { return false; }

  // Method for painting a control. The rect is in zoomed coordinates.
  virtual void Paint(ControlPart,
                     ControlStates,
                     GraphicsContext&,
                     const IntRect& /*zoomedRect*/,
                     float /*zoomFactor*/,
                     ScrollableArea*) const {}

  // Add visual overflow (e.g., the check on an OS X checkbox). The rect passed
  // in is in zoomed coordinates so the inflation should take that into account
  // and make sure the inflation amount is also scaled by the zoomFactor.
  virtual void AddVisualOverflow(ControlPart,
                                 ControlStates,
                                 float zoom_factor,
                                 IntRect& border_box) const {}

 private:
  mutable Color active_selection_color_;
  mutable Color inactive_selection_color_;
};

PLATFORM_EXPORT Theme* PlatformTheme();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_H_
