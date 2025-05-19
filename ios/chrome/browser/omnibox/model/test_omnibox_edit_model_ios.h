// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_EDIT_MODEL_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_EDIT_MODEL_IOS_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

class TestOmniboxEditModelIOS : public OmniboxEditModelIOS {
 public:
  TestOmniboxEditModelIOS(OmniboxControllerIOS* omnibox_controller,
                          OmniboxViewIOS* view,
                          PrefService* pref_service);
  ~TestOmniboxEditModelIOS() override;
  TestOmniboxEditModelIOS(const TestOmniboxEditModelIOS&) = delete;
  TestOmniboxEditModelIOS& operator=(const TestOmniboxEditModelIOS&) = delete;

  // OmniboxEditModel:
  bool PopupIsOpen() const override;
  AutocompleteMatch CurrentMatch(GURL* alternate_nav_url) const override;

  void SetPopupIsOpen(bool open);

  void SetCurrentMatchForTest(const AutocompleteMatch& match);

  void OnPopupDataChanged(const std::u16string& inline_autocompletion,
                          const std::u16string& additional_text,
                          const AutocompleteMatch& match) override;

  const std::u16string& text() const { return text_; }

 protected:
  PrefService* GetPrefService() override;
  const PrefService* GetPrefService() const override;

 private:
  bool popup_is_open_;
  std::unique_ptr<AutocompleteMatch> override_current_match_;

  // Contains the most recent text passed by the popup model to the edit model.
  std::u16string text_;
  raw_ptr<PrefService> pref_service_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_EDIT_MODEL_IOS_H_
