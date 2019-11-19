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

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

float PagePopupClient::ZoomFactor() {
  if (const ComputedStyle* style = OwnerElement().GetComputedStyle())
    return style->EffectiveZoom();
  if (LocalFrame* frame = OwnerElement().GetDocument().GetFrame())
    return frame->PageZoomFactor();
  return 1;
}

float PagePopupClient::ScaledZoomFactor() {
  float scale_factor = GetChromeClient().WindowToViewportScalar(
      OwnerElement().GetDocument().GetFrame(), 1.0f);
  return ZoomFactor() / scale_factor;
}

#define addLiteral(literal, data) data->Append(literal, sizeof(literal) - 1)

void PagePopupClient::AddJavaScriptString(const String& str,
                                          SharedBuffer* data) {
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
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": ", data);
  AddJavaScriptString(value, data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  int value,
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": ", data);
  AddString(String::Number(value), data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  unsigned value,
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": ", data);
  AddString(String::Number(value), data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  bool value,
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": ", data);
  if (value)
    addLiteral("true", data);
  else
    addLiteral("false", data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  double value,
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": ", data);
  AddString(String::Number(value), data);
  addLiteral(",\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  const Vector<String>& values,
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": [", data);
  for (unsigned i = 0; i < values.size(); ++i) {
    if (i)
      addLiteral(",", data);
    AddJavaScriptString(values[i], data);
  }
  addLiteral("],\n", data);
}

void PagePopupClient::AddProperty(const char* name,
                                  const IntRect& rect,
                                  SharedBuffer* data) {
  data->Append(name, strlen(name));
  addLiteral(": {", data);
  AddProperty("x", rect.X(), data);
  AddProperty("y", rect.Y(), data);
  AddProperty("width", rect.Width(), data);
  AddProperty("height", rect.Height(), data);
  addLiteral("},\n", data);
}

}  // namespace blink
