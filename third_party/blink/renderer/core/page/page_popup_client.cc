/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/page/page_popup_client.h"

#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

float PagePopupClient::ZoomFactor() {
  if (const ComputedStyle* style = OwnerElement().GetComputedStyle())
    return style->EffectiveZoom();
  if (LocalFrame* frame = OwnerElement().GetDocument().GetFrame())
    return frame->LayoutZoomFactor();
  return 1;
}

float PagePopupClient::ScaledZoomFactor() {
  float scale_factor = GetChromeClient().WindowToViewportScalar(
      OwnerElement().GetDocument().GetFrame(), 1.0f);
  return ZoomFactor() / scale_factor;
}

#define addLiteral(literal, data) data.Append(literal, sizeof(literal) - 1)

void PagePopupClient::AddJavaScriptString(const String& str,
                                          SegmentedBuffer& data) {
  addLiteral("\"", data);
  StringBuilder builder;
  builder.ReserveCapacity(str.length());
  for (unsigned i = 0; i < str.length(); ++i) {
    if (str[i] == '\r') {
      builder.Append("\\r");
    } else if (str[i] == '\n') {
      builder.Append("\\n");
    } else if (str[i] == '\\' || str[i] == '"') {
      builder.Append('\\');
      builder.Append(str[i]);
    } else if (str[i] == '<') {
      // Need to avoid to add "</script>" because the resultant string is
      // typically embedded in <script>.
      builder.Append("\\x3C");
    } else if (str[i] < 0x20 || str[i] == kLineSeparator ||
               str[i] == kParagraphSeparator) {
      builder.AppendFormat("\\u%04X", str[i]);
    } else {
      builder.Append(str[i]);
    }
  }
  AddString(builder.ToString(), data);
  addLiteral("\"", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  const String& value,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": ", data);
  AddJavaScriptString(value, data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  int value,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": ", data);
  AddString(String::Number(value), data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  unsigned value,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": ", data);
  AddString(String::Number(value), data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  bool value,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": ", data);
  if (value)
    addLiteral("true", data);
  else
    addLiteral("false", data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  double value,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": ", data);
  AddString(String::Number(value), data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  const Vector<String>& values,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": [", data);
  for (unsigned i = 0; i < values.size(); ++i) {
    if (i)
      addLiteral(",", data);
    AddJavaScriptString(values[i], data);
  }
  addLiteral("],\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  const gfx::Rect& rect,
                                  SegmentedBuffer& data) {
  data.Append(name, strlen(name));
  addLiteral(": {", data);
  AddProperty("x", rect.x(), data);
  AddProperty("y", rect.y(), data);
  AddProperty("width", rect.width(), data);
  AddProperty("height", rect.height(), data);
  addLiteral("},\n", data);
}

void PagePopupClient::AddLocalizedProperty(const char* name,
                                           int resource_id,
                                           SegmentedBuffer& data) {
  AddProperty(name, GetLocale().QueryString(resource_id), data);
}

CSSFontSelector* PagePopupClient::CreateCSSFontSelector(
    Document& popup_document) {
  return MakeGarbageCollected<CSSFontSelector>(popup_document);
}

PagePopupController* PagePopupClient::CreatePagePopupController(
    Page& page,
    PagePopup& popup) {
  return MakeGarbageCollected<PagePopupController>(page, popup, this);
}

void PagePopupClient::AdjustSettingsFromOwnerColorScheme(
    Settings& popup_settings) {
  // Color picker and and date/time chooser popups use HTML/CSS/javascript to
  // implement the UI. They are themed light or dark based on media queries in
  // the CSS. Whether the control is styled light or dark can be selected using
  // the color-scheme property on the input element independently from the
  // preferred color-scheme of the input's document.
  //
  // To affect the media queries inside the popup accordingly, we set the
  // preferred color-scheme inside the popup to the used color-scheme for the
  // input element, and disable forced darkening.

  popup_settings.SetForceDarkModeEnabled(false);

  if (const auto* style = OwnerElement().GetComputedStyle()) {
    // The style can be out-of-date if e.g. a key event handler modified the
    // OwnerElement()'s style before the default handler started opening the
    // popup. If the key handler forced a style update the style may be
    // up-to-date and null. Note that if there's a key event handler which
    // changes the color-scheme between the key is pressed and the popup is
    // opened, the color-scheme of the form element and its popup may not match.
    // If we think it's important to have an up-to-date style here, we need to
    // run an UpdateStyleAndLayoutTree() before opening the popup in the various
    // default event handlers.
    //
    // Avoid using dark color scheme stylesheet for popups when forced colors
    // mode is active.
    // TODO(iopopesc): move this to popup CSS when the ForcedColors feature is
    // enabled by default.
    bool in_forced_colors_mode =
        OwnerElement().GetDocument().InForcedColorsMode();
    popup_settings.SetPreferredColorScheme(
        style->UsedColorScheme() == mojom::blink::ColorScheme::kDark &&
                !in_forced_colors_mode
            ? mojom::blink::PreferredColorScheme::kDark
            : mojom::blink::PreferredColorScheme::kLight);
  }
}

}  // namespace blink
