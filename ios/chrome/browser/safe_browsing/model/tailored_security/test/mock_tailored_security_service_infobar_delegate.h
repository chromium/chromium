// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TEST_MOCK_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TEST_MOCK_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_infobar_delegate.h"
#import "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

// Mock infobar delegate for tailored security service.
class MockTailoredSecurityServiceInfobarDelegate
    : public TailoredSecurityServiceInfobarDelegate {
 public:
  MockTailoredSecurityServiceInfobarDelegate(
      TailoredSecurityServiceMessageState message_state,
      web::WebState* web_state);
  ~MockTailoredSecurityServiceInfobarDelegate() override;

  // Factory method that creates a mock tailored security service delegate..
  static std::unique_ptr<MockTailoredSecurityServiceInfobarDelegate> Create(
      TailoredSecurityServiceMessageState message_state,
      web::WebState* web_state);
  MOCK_METHOD0(InfoBarDismissed, void());
  MOCK_METHOD0(Cancel, bool());
  MOCK_METHOD1(InfobarPresenting, void(bool automatic));
  MOCK_METHOD0(InfobarDismissed, void());
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TEST_MOCK_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_
