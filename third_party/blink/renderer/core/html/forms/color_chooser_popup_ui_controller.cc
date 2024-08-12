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

#include "build/build_config.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/chooser_resource_loader.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/color_page_popup_controller.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ax_strings.h"

namespace blink {

// Keep in sync with Actions in colorSuggestionPicker.js.
enum ColorPickerPopupAction {
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
      locale_(Locale::DefaultLocale()),
      eye_dropper_chooser_(frame->DomWindow()) {}

ColorChooserPopupUIController::~ColorChooserPopupUIController() {
  DCHECK(!popup_);
}

void ColorChooserPopupUIController::Trace(Visitor* visitor) const {
  visitor->Trace(chrome_client_);
  visitor->Trace(eye_dropper_chooser_);
  ColorChooserUIController::Trace(visitor);
}

void ColorChooserPopupUIController::OpenUI() {
  OpenPopup();
}

void ColorChooserPopupUIController::EndChooser() {
  ColorChooserUIController::EndChooser();
  CancelPopup();
}

AXObject* ColorChooserPopupUIController::RootAXObject(Element* popup_owner) {
  return popup_ ? popup_->RootAXObject(popup_owner) : nullptr;
}

void ColorChooserPopupUIController::WriteDocument(SegmentedBuffer& data) {
  if (client_->ShouldShowSuggestions()) {
    WriteColorSuggestionPickerDocument(data);
  } else {
    WriteColorPickerDocument(data);
  }
}

void ColorChooserPopupUIController::WriteColorPickerDocument(
    SegmentedBuffer& data) {
  gfx::Rect anchor_rect_in_screen = chrome_client_->LocalRootToScreenDIPs(
      client_->ElementRectRelativeToLocalRoot(), frame_->View());

  PagePopupClient::AddString(
      "<!DOCTYPE html><head><meta charset='UTF-8'><meta name='color-scheme' "
      "content='light dark'><style>\n",
      data);
  data.Append(ChooserResourceLoader::GetPickerCommonStyleSheet());
  data.Append(ChooserResourceLoader::GetColorPickerStyleSheet());
  PagePopupClient::AddString(
      "</style></head><body>\n"
      "<div id='main'>Loading...</div><script>\n"
      "window.dialogArguments = {\n",
      data);
  PagePopupClient::AddProperty(
      "selectedColor", client_->CurrentColor().SerializeAsCSSColor(), data);
  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data);
  AddProperty("zoomFactor", ScaledZoomFactor(), data);
  AddProperty("shouldShowColorSuggestionPicker", false, data);
  AddProperty("isEyeDropperEnabled", ::features::IsEyeDropperEnabled(), data);
#if BUILDFLAG(IS_MAC)
  AddProperty("isBorderTransparent", true, data);
#endif
  // We don't create PagePopups on Android, so these strings are excluded
  // from blink_strings.grd on Android to save binary size.  We have to
  // exclude them here as well to avoid an Android build break.
#if !BUILDFLAG(IS_ANDROID)
  AddLocalizedProperty("axColorWellLabel", IDS_AX_COLOR_WELL, data);
  AddLocalizedProperty("axColorWellRoleDescription",
                       IDS_AX_COLOR_WELL_ROLEDESCRIPTION, data);
  AddLocalizedProperty("axHueSliderLabel", IDS_AX_COLOR_HUE_SLIDER, data);
  AddLocalizedProperty("axHexadecimalEditLabel", IDS_AX_COLOR_EDIT_HEXADECIMAL,
                       data);
  AddLocalizedProperty("axRedEditLabel", IDS_AX_COLOR_EDIT_RED, data);
  AddLocalizedProperty("axGreenEditLabel", IDS_AX_COLOR_EDIT_GREEN, data);
  AddLocalizedProperty("axBlueEditLabel", IDS_AX_COLOR_EDIT_BLUE, data);
  AddLocalizedProperty("axHueEditLabel", IDS_AX_COLOR_EDIT_HUE, data);
  AddLocalizedProperty("axSaturationEditLabel", IDS_AX_COLOR_EDIT_SATURATION,
                       data);
  AddLocalizedProperty("axLightnessEditLabel", IDS_AX_COLOR_EDIT_LIGHTNESS,
                       data);
  AddLocalizedProperty("axFormatTogglerLabel", IDS_AX_COLOR_FORMAT_TOGGLER,
                       data);
  AddLocalizedProperty("axEyedropperLabel", IDS_AX_COLOR_EYEDROPPER, data);
#else
  CHECK(false) << "We should never reach PagePopupClient code on Android";
#endif
  PagePopupClient::AddString("};\n", data);
  data.Append(ChooserResourceLoader::GetPickerCommonJS());
  data.Append(ChooserResourceLoader::GetColorPickerJS());
  data.Append(ChooserResourceLoader::GetColorPickerCommonJS());
  PagePopupClient::AddString("</script></body>\n", data);
}

void ColorChooserPopupUIController::WriteColorSuggestionPickerDocument(
    SegmentedBuffer& data) {
  DCHECK(client_->ShouldShowSuggestions());

  Vector<String> suggestion_values;
  for (auto& suggestion : client_->Suggestions()) {
    // TODO(https://crbug.com/1351544): ColorSuggestions be sent as Color or
    // SkColor4f and should be serialized as CSS colors.
    suggestion_values.push_back(
        Color::FromRGBA32(suggestion->color).SerializeAsCanvasColor());
  }
  gfx::Rect anchor_rect_in_screen = chrome_client_->LocalRootToScreenDIPs(
      client_->ElementRectRelativeToLocalRoot(), frame_->View());

  PagePopupClient::AddString(
      "<!DOCTYPE html><head><meta charset='UTF-8'><meta name='color-scheme' "
      "content='light dark'><style>\n",
      data);
  data.Append(ChooserResourceLoader::GetPickerCommonStyleSheet());
  data.Append(ChooserResourceLoader::GetColorSuggestionPickerStyleSheet());
  data.Append(ChooserResourceLoader::GetColorPickerStyleSheet());
  PagePopupClient::AddString(
      "</style></head><body>\n"
      "<div id='main'>Loading...</div><script>\n"
      "window.dialogArguments = {\n",
      data);
  PagePopupClient::AddProperty("values", suggestion_values, data);
  PagePopupClient::AddLocalizedProperty("otherColorLabel",
                                        IDS_FORM_OTHER_COLOR_LABEL, data);
  PagePopupClient::AddProperty(
      "selectedColor", client_->CurrentColor().SerializeAsCSSColor(), data);
  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data);
  AddProperty("zoomFactor", ScaledZoomFactor(), data);
  AddProperty("shouldShowColorSuggestionPicker", true, data);
  AddProperty("isEyeDropperEnabled", ::features::IsEyeDropperEnabled(), data);
#if BUILDFLAG(IS_MAC)
  AddProperty("isBorderTransparent", true, data);
#endif
  PagePopupClient::AddString("};\n", data);
  data.Append(ChooserResourceLoader::GetPickerCommonJS());
  data.Append(ChooserResourceLoader::GetColorSuggestionPickerJS());
  data.Append(ChooserResourceLoader::GetColorPickerJS());
  data.Append(ChooserResourceLoader::GetColorPickerCommonJS());
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
  CancelPopup();
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
  eye_dropper_chooser_.reset();

  if (!chooser_)
    EndChooser();
}

Element& ColorChooserPopupUIController::OwnerElement() {
  return client_->OwnerElement();
}

ChromeClient& ColorChooserPopupUIController::GetChromeClient() {
  return *chrome_client_;
}

void ColorChooserPopupUIController::OpenPopup() {
  DCHECK(!popup_);
  popup_ = chrome_client_->OpenPagePopup(this);
}

void ColorChooserPopupUIController::CancelPopup() {
  if (!popup_)
    return;
  chrome_client_->ClosePagePopup(popup_);
}

PagePopupController* ColorChooserPopupUIController::CreatePagePopupController(
    Page& page,
    PagePopup& popup) {
  return MakeGarbageCollected<ColorPagePopupController>(page, popup, this);
}

void ColorChooserPopupUIController::EyeDropperResponseHandler(bool success,
                                                              uint32_t color) {
  eye_dropper_chooser_.reset();

  if (!popup_)
    return;
  // Notify the popup that there is a response from the eye dropper.
  SegmentedBuffer data;
  PagePopupClient::AddString("window.updateData = {\n", data);
  AddProperty("success", success, data);
  // TODO(https://crbug.com/1351544): The EyeDropper should use Color or
  // SkColor4f.
  AddProperty("color", Color::FromRGBA32(color).SerializeAsCSSColor(), data);
  PagePopupClient::AddString("}\n", data);
  Vector<char> flatten_data = std::move(data).CopyAs<Vector<char>>();
  popup_->PostMessageToPopup(
      String::FromUTF8(base::as_string_view(flatten_data)));
}

void ColorChooserPopupUIController::OpenEyeDropper() {
  // Don't open the eye dropper without user activation or if it is already
  // opened.
  if (!LocalFrame::HasTransientUserActivation(frame_) ||
      eye_dropper_chooser_.is_bound())
    return;

  frame_->GetBrowserInterfaceBroker().GetInterface(
      eye_dropper_chooser_.BindNewPipeAndPassReceiver(
          frame_->GetTaskRunner(TaskType::kUserInteraction)));
  eye_dropper_chooser_.set_disconnect_handler(WTF::BindOnce(
      &ColorChooserPopupUIController::EndChooser, WrapWeakPersistent(this)));
  eye_dropper_chooser_->Choose(
      WTF::BindOnce(&ColorChooserPopupUIController::EyeDropperResponseHandler,
                    WrapWeakPersistent(this)));
}

void ColorChooserPopupUIController::AdjustSettings(Settings& popup_settings) {
  AdjustSettingsFromOwnerColorScheme(popup_settings);
}

}  // namespace blink
