/*
 * Copyright (C) 2010 Joel Stanley. All rights reserved.
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

#include "third_party/blink/public/web/web_render_theme.h"

#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_theme_default.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

void SetCaretBlinkInterval(base::TimeDelta interval) {
  LayoutTheme::GetTheme().SetCaretBlinkInterval(interval);
}

void SetFocusRingColor(SkColor color) {
  LayoutTheme::GetTheme().SetCustomFocusRingColor(color);
}

void SetSelectionColors(unsigned active_background_color,
                        unsigned active_foreground_color,
                        unsigned inactive_background_color,
                        unsigned inactive_foreground_color) {
  LayoutTheme::GetTheme().SetSelectionColors(
      active_background_color, active_foreground_color,
      inactive_background_color, inactive_foreground_color);
}

void SystemColorsChanged() {
  LayoutTheme::GetTheme().PlatformColorsDidChange();
}

void ColorSchemeChanged() {
  LayoutTheme::GetTheme().ColorSchemeDidChange();
}

}  // namespace blink
