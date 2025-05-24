// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/test_omnibox_edit_model_ios.h"

#import <memory>

#import "components/omnibox/browser/test_omnibox_client.h"

TestOmniboxEditModelIOS::TestOmniboxEditModelIOS(
    OmniboxControllerIOS* omnibox_controller,
    OmniboxViewIOS* view,
    PrefService* pref_service)
    : OmniboxEditModelIOS(omnibox_controller, view),
      popup_is_open_(false),
      pref_service_(pref_service) {}

TestOmniboxEditModelIOS::~TestOmniboxEditModelIOS() = default;

bool TestOmniboxEditModelIOS::PopupIsOpen() const {
  return popup_is_open_;
}

AutocompleteMatch TestOmniboxEditModelIOS::CurrentMatch(
    GURL* alternate_nav_url) const {
  if (override_current_match_) {
    return *override_current_match_;
  }

  return OmniboxEditModelIOS::CurrentMatch(alternate_nav_url);
}

void TestOmniboxEditModelIOS::SetPopupIsOpen(bool open) {
  popup_is_open_ = open;
}

void TestOmniboxEditModelIOS::SetCurrentMatchForTest(
    const AutocompleteMatch& match) {
  override_current_match_ = std::make_unique<AutocompleteMatch>(match);
}

void TestOmniboxEditModelIOS::OnPopupDataChanged(
    const std::u16string& inline_autocompletion,
    const std::u16string& additional_text,
    const AutocompleteMatch& match) {
  OmniboxEditModelIOS::OnPopupDataChanged(inline_autocompletion,
                                          additional_text, match);
  text_ = inline_autocompletion;
}

PrefService* TestOmniboxEditModelIOS::GetPrefService() {
  return const_cast<PrefService*>(
      const_cast<const TestOmniboxEditModelIOS*>(this)->GetPrefService());
}

const PrefService* TestOmniboxEditModelIOS::GetPrefService() const {
  return pref_service_ == nullptr ? OmniboxEditModelIOS::GetPrefService()
                                  : pref_service_.get();
}
