// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_MOCK_INTERSTITIAL_DELEGATE_H_
#define IOS_WEB_TEST_FAKES_MOCK_INTERSTITIAL_DELEGATE_H_

#import "ios/web/public/security/web_interstitial_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

// A mock html web interstitial delegate.
class MockInterstitialDelegate : public web::WebInterstitialDelegate {
 public:
  MockInterstitialDelegate();
  ~MockInterstitialDelegate();

  // HtmlWebMockInterstitialDelegate overrides
  MOCK_METHOD0(OnProceed, void());
  MOCK_METHOD0(OnDontProceed, void());
  std::string GetHtmlContents() const override { return ""; }
};

#endif  // IOS_WEB_TEST_FAKES_MOCK_INTERSTITIAL_DELEGATE_H_
