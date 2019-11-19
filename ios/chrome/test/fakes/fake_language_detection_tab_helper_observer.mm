// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_language_detection_tab_helper_observer.h"

#import "base/logging.h"
#include "components/translate/core/common/language_detection_details.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeLanguageDetectionTabHelperObserver::FakeLanguageDetectionTabHelperObserver(
    web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state);
  language::IOSLanguageDetectionTabHelper* language_detection_tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state_);
  language_detection_tab_helper->AddObserver(this);
}

FakeLanguageDetectionTabHelperObserver::
    ~FakeLanguageDetectionTabHelperObserver() {
  if (web_state_) {
    StopObservingIOSLanguageDetectionTabHelper();
  }
}

void FakeLanguageDetectionTabHelperObserver::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  language_detection_details_ =
      std::make_unique<translate::LanguageDetectionDetails>(details);
}

void FakeLanguageDetectionTabHelperObserver::
    IOSLanguageDetectionTabHelperWasDestroyed(
        language::IOSLanguageDetectionTabHelper* tab_helper) {
  StopObservingIOSLanguageDetectionTabHelper();
}

translate::LanguageDetectionDetails*
FakeLanguageDetectionTabHelperObserver::GetLanguageDetectionDetails() {
  return language_detection_details_.get();
}

void FakeLanguageDetectionTabHelperObserver::ResetLanguageDetectionDetails() {
  language_detection_details_.reset();
}

void FakeLanguageDetectionTabHelperObserver::
    StopObservingIOSLanguageDetectionTabHelper() {
  DCHECK(web_state_);

  language::IOSLanguageDetectionTabHelper* language_detection_tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(web_state_);
  language_detection_tab_helper->RemoveObserver(this);

  web_state_ = nullptr;
}
