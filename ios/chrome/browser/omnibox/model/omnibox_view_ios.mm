// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"

#import <CoreText/CoreText.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <string>

#import "base/command_line.h"
#import "base/ios/device_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_text_util.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/public/omnibox_metrics_helper.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/page_transition_types.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/window_open_disposition.h"
#import "ui/gfx/image/image.h"

using base::UserMetricsAction;

#pragma mark - Public

OmniboxViewIOS::OmniboxViewIOS(OmniboxTextFieldIOS* field) : field_(field) {}

OmniboxViewIOS::~OmniboxViewIOS() = default;

void OmniboxViewIOS::SetOmniboxEditModel(OmniboxEditModelIOS* edit_model) {
  model_ = edit_model->AsWeakPtr();
}

void OmniboxViewIOS::SetOmniboxController(
    OmniboxControllerIOS* omnibox_controller) {
  controller_ = omnibox_controller->AsWeakPtr();
}

std::u16string OmniboxViewIOS::GetText() const {
  return base::SysNSStringToUTF16([field_ displayedText]);
}

void OmniboxViewIOS::SetUserText(const std::u16string& text) {
  SetUserText(text, true);
}

void OmniboxViewIOS::SetUserText(const std::u16string& text,
                                 bool update_popup) {
  if (model_) {
    model_->SetUserText(text);
  }
  SetWindowTextAndCaretPos(text, text.length(), update_popup, true);
}

void OmniboxViewIOS::SetWindowTextAndCaretPos(const std::u16string& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  [omnibox_text_controller_ setWindowText:text
                                 caretPos:caret_pos
                        startAutocomplete:update_popup
                        notifyTextChanged:notify_text_changed];
}

void OmniboxViewIOS::SetCaretPos(size_t caret_pos) {
  [omnibox_text_controller_ setCaretPos:caret_pos];
}

void OmniboxViewIOS::UpdatePopup() {
  [omnibox_text_controller_ startAutocompleteAfterEdit];
}

void OmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& user_text,
    const std::u16string& inline_autocompletion) {
  [omnibox_text_controller_
      updateAutocompleteIfTextChanged:user_text
                       autocompletion:inline_autocompletion];
}

void OmniboxViewIOS::SetAdditionalText(const std::u16string& text) {
  [omnibox_text_controller_ setAdditionalText:text];
}

void OmniboxViewIOS::OnAcceptAutocomplete() {
  [omnibox_text_controller_ onAcceptAutocomplete];
}
