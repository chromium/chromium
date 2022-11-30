// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/fake_translate_overlay_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
