// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_POPUP_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_POPUP_VIEW_IOS_H_

#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_ios.h"

// Fake implementation of OmniboxPopupViewIOS for use in tests.
class TestOmniboxPopupViewIOS : public OmniboxPopupViewIOS {
 public:
  TestOmniboxPopupViewIOS()
      : OmniboxPopupViewIOS(/*controller=*/nullptr,
                            /*omnibox_autocomplete_controller=*/nil) {}
  ~TestOmniboxPopupViewIOS() override = default;
  bool IsOpen() const override;
  void UpdatePopupAppearance() override {}
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_POPUP_VIEW_IOS_H_
