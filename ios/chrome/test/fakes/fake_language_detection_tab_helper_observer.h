// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_LANGUAGE_DETECTION_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_LANGUAGE_DETECTION_TAB_HELPER_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"

namespace web {
class WebState;
}  // namespace web

// Gets notified when translate::LanguageDetectionDetails becomes available for
// the given web state.
class FakeLanguageDetectionTabHelperObserver
    : public language::IOSLanguageDetectionTabHelper::Observer {
 public:
  FakeLanguageDetectionTabHelperObserver(web::WebState* web_state);
  ~FakeLanguageDetectionTabHelperObserver() override;

  // language::IOSLanguageDetectionTabHelper::Observer
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void IOSLanguageDetectionTabHelperWasDestroyed(
      language::IOSLanguageDetectionTabHelper* tab_helper) override;

  translate::LanguageDetectionDetails* GetLanguageDetectionDetails();
  void ResetLanguageDetectionDetails();

 private:
  web::WebState* web_state_;
  std::unique_ptr<translate::LanguageDetectionDetails>
      language_detection_details_;

  // Stops observing the IOSLanguageDetectionTabHelper instance associated with
  // |web_state_| and sets |web_state_| to null.
  void StopObservingIOSLanguageDetectionTabHelper();

  DISALLOW_COPY_AND_ASSIGN(FakeLanguageDetectionTabHelperObserver);
};

#endif  // IOS_CHROME_TEST_FAKES_FAKE_LANGUAGE_DETECTION_TAB_HELPER_OBSERVER_H_
