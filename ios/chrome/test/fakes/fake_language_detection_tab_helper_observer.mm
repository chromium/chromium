// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_language_detection_tab_helper_observer.h"

#import "base/logging.h"
#import "components/translate/core/common/language_detection_details.h"

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
    StopObservingIOSLanguageDetectionTabHelper(
        language::IOSLanguageDetectionTabHelper::FromWebState(web_state_));
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
  StopObservingIOSLanguageDetectionTabHelper(tab_helper);
}

translate::LanguageDetectionDetails*
FakeLanguageDetectionTabHelperObserver::GetLanguageDetectionDetails() {
  return language_detection_details_.get();
}

void FakeLanguageDetectionTabHelperObserver::ResetLanguageDetectionDetails() {
  language_detection_details_.reset();
}

void FakeLanguageDetectionTabHelperObserver::
    StopObservingIOSLanguageDetectionTabHelper(
        language::IOSLanguageDetectionTabHelper* tab_helper) {
  DCHECK(web_state_);

  if (tab_helper) {
    tab_helper->RemoveObserver(this);
  }

  web_state_ = nullptr;
}
