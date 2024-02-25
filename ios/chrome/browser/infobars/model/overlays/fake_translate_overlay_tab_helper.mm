// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/fake_translate_overlay_tab_helper.h"

FakeTranslateOverlayTabHelper::FakeTranslateOverlayTabHelper(
    web::WebState* web_state)
    : TranslateOverlayTabHelper(web_state) {}

void FakeTranslateOverlayTabHelper::CallTranslationFinished(bool success) {
  for (auto& observer : observers_) {
    observer.TranslationFinished(this, success);
  }
}
void FakeTranslateOverlayTabHelper::CallTranslateOverlayTabHelperDestroyed() {
  for (auto& observer : observers_) {
    observer.TranslateOverlayTabHelperDestroyed(this);
  }
}
