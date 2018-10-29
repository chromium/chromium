/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/color_chooser_popup_ui_controller.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

// Keep in sync with Actions in colorSuggestionPicker.js.
enum ColorPickerPopupAction {
  kColorPickerPopupActionChooseOtherColor = -2,
  kColorPickerPopupActionCancel = -1,
  kColorPickerPopupActionSetValue = 0
};

ColorChooserPopupUIController::ColorChooserPopupUIController(
    LocalFrame* frame,
    ChromeClient* chrome_client,
    blink::ColorChooserClient* client)
    : ColorChooserUIController(frame, client),
      chrome_client_(chrome_client),
      popup_(nullptr),
      locale_(Locale::DefaultLocale()) {}

ColorChooserPopupUIController::~ColorChooserPopupUIController() = default;

void ColorChooserPopupUIController::Dispose() {
  // Finalized earlier so as to access chrome_client_ while alive.
  ClosePopup();
  // ~ColorChooserUIController calls EndChooser().
}

void ColorChooserPopupUIController::Trace(blink::Visitor* visitor) {
  visitor->Trace(chrome_client_);
  ColorChooserUIController::Trace(visitor);
}

void ColorChooserPopupUIController::OpenUI() {
  if (client_->ShouldShowSuggestions())
    OpenPopup();
  else
    OpenColorChooser();
}

void ColorChooserPopupUIController::EndChooser() {
  ColorChooserUIController::EndChooser();
  ClosePopup();
}

AXObject* ColorChooserPopupUIController::RootAXObject() {
  return popup_ ? popup_->RootAXObject() : nullptr;
}

void ColorChooserPopupUIController::WriteDocument(SharedBuffer* data) {
  Vector<String> suggestion_values;
  for (auto& suggestion : client_->Suggestions())
    suggestion_values.push_back(Color(suggestion->color).Serialized());
  IntRect anchor_rect_in_screen = chrome_client_->ViewportToScreen(
      client_->ElementRectRelativeToViewport(), frame_->View());

  PagePopupClient::AddString(
      "<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", data);
  data->Append(Platform::Current()->GetDataResource("pickerCommon.css"));
  data->Append(
      Platform::Current()->GetDataResource("colorSuggestionPicker.css"));
  PagePopupClient::AddString(
      "</style></head><body><div id=main>Loading...</div><script>\n"
      "window.dialogArguments = {\n",
      data);
  PagePopupClient::AddProperty("values", suggestion_values, data);
  PagePopupClient::AddProperty(
      "otherColorLabel",
      GetLocale().QueryString(WebLocalizedString::kOtherColorLabel), data);
  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data);
  AddProperty("zoomFactor", ZoomFactor(), data);
  PagePopupClient::AddString("};\n", data);
  data->Append(Platform::Current()->GetDataResource("pickerCommon.js"));
  data->Append(
      Platform::Current()->GetDataResource("colorSuggestionPicker.js"));
  PagePopupClient::AddString("</script></body>\n", data);
}

Locale& ColorChooserPopupUIController::GetLocale() {
  return locale_;
}

void ColorChooserPopupUIController::SetValueAndClosePopup(
    int num_value,
    const String& string_value) {
  DCHECK(popup_);
  DCHECK(client_);
  if (num_value == kColorPickerPopupActionSetValue)
    SetValue(string_value);
  if (num_value == kColorPickerPopupActionChooseOtherColor)
    OpenColorChooser();
  ClosePopup();
}

void ColorChooserPopupUIController::SetValue(const String& value) {
  DCHECK(client_);
  Color color;
  bool success = color.SetFromString(value);
  DCHECK(success);
  client_->DidChooseColor(color);
}

void ColorChooserPopupUIController::DidClosePopup() {
  popup_ = nullptr;

  if (!chooser_)
    EndChooser();
}

Element& ColorChooserPopupUIController::OwnerElement() {
  return client_->OwnerElement();
}

void ColorChooserPopupUIController::OpenPopup() {
  DCHECK(!popup_);
  popup_ = chrome_client_->OpenPagePopup(this);
}

void ColorChooserPopupUIController::ClosePopup() {
  if (!popup_)
    return;
  chrome_client_->ClosePagePopup(popup_);
}

}  // namespace blink
