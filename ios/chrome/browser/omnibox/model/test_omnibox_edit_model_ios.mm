// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/test_omnibox_edit_model_ios.h"

#import <memory>

#import "components/omnibox/browser/test_omnibox_client.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"

TestOmniboxEditModelIOS::TestOmniboxEditModelIOS(
    OmniboxControllerIOS* omnibox_controller,
    OmniboxClient* omnibox_client,
    PrefService* pref_service,
    OmniboxTextModel* text_model,
    OmniboxMetricsRecorder* omnibox_metrics_recorder)
    : OmniboxEditModelIOS(omnibox_controller,
                          omnibox_client,
                          text_model,
                          omnibox_metrics_recorder),
      popup_is_open_(false),
      pref_service_(pref_service) {}

TestOmniboxEditModelIOS::~TestOmniboxEditModelIOS() = default;

bool TestOmniboxEditModelIOS::PopupIsOpen() const {
  return popup_is_open_;
}

void TestOmniboxEditModelIOS::SetPopupIsOpen(bool open) {
  popup_is_open_ = open;
}

PrefService* TestOmniboxEditModelIOS::GetPrefService() {
  return const_cast<PrefService*>(
      const_cast<const TestOmniboxEditModelIOS*>(this)->GetPrefService());
}

const PrefService* TestOmniboxEditModelIOS::GetPrefService() const {
  return pref_service_ == nullptr ? OmniboxEditModelIOS::GetPrefService()
                                  : pref_service_.get();
}
