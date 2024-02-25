// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_FAKE_TRANSLATE_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_FAKE_TRANSLATE_OVERLAY_TAB_HELPER_H_

#import "ios/chrome/browser/infobars/model/overlays/translate_overlay_tab_helper.h"

// Fake TranslateOverlayTabHelper for use in tests to simulate callbacks from
// changes to the state of Translate
class FakeTranslateOverlayTabHelper : public TranslateOverlayTabHelper {
 public:
  ~FakeTranslateOverlayTabHelper() override {}

  // Trigger methods for Observer TranslateOverlayTabHelper::callbacks
  void CallTranslationFinished(bool success);
  void CallTranslateOverlayTabHelperDestroyed();

 private:
  explicit FakeTranslateOverlayTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<FakeTranslateOverlayTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_FAKE_TRANSLATE_OVERLAY_TAB_HELPER_H_
