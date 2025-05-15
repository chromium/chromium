// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_POPUP_VIEW_BASE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_POPUP_VIEW_BASE_H_

#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_base.h"

// Fake implementation of OmniboxPopupViewBase for use in tests.
class TestOmniboxPopupViewBase : public OmniboxPopupViewBase {
 public:
  TestOmniboxPopupViewBase() : OmniboxPopupViewBase(/*controller=*/nullptr) {}
  ~TestOmniboxPopupViewBase() override = default;
  bool IsOpen() const override;
  void UpdatePopupAppearance() override {}
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_TEST_OMNIBOX_POPUP_VIEW_BASE_H_
