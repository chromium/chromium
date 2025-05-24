// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_ios.h"

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    OmniboxControllerIOS* controller,
    OmniboxAutocompleteController* omnibox_autocomplete_controller)
    : controller_(controller),
      omnibox_autocomplete_controller_(omnibox_autocomplete_controller) {
  DCHECK(controller);
  model()->set_popup_view(this);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  model()->set_popup_view(nullptr);
}

OmniboxEditModelIOS* OmniboxPopupViewIOS::model() {
  return const_cast<OmniboxEditModelIOS*>(
      const_cast<const OmniboxPopupViewIOS*>(this)->model());
}

const OmniboxEditModelIOS* OmniboxPopupViewIOS::model() const {
  return controller_->edit_model();
}

OmniboxControllerIOS* OmniboxPopupViewIOS::controller() {
  return const_cast<OmniboxControllerIOS*>(
      const_cast<const OmniboxPopupViewIOS*>(this)->controller());
}

const OmniboxControllerIOS* OmniboxPopupViewIOS::controller() const {
  return controller_;
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  [omnibox_autocomplete_controller_ updatePopupSuggestions];
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return omnibox_autocomplete_controller_.hasSuggestions;
}
