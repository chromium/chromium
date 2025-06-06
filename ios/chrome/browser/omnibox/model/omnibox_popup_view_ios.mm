// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_ios.h"

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    OmniboxEditModelIOS* omnibox_edit_model,
    OmniboxAutocompleteController* omnibox_autocomplete_controller)
    : omnibox_autocomplete_controller_(omnibox_autocomplete_controller) {
  model_ = omnibox_edit_model->AsWeakPtr();
  model_->set_popup_view(this);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  model_->set_popup_view(nullptr);
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return omnibox_autocomplete_controller_.hasSuggestions;
}
