// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_TEST_MOCK_USER_CONSENT_HANDLER_H_
#define CONTENT_BROWSER_SMS_TEST_MOCK_USER_CONSENT_HANDLER_H_

#include "content/browser/sms/user_consent_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockUserConsentHandler : public UserConsentHandler {
 public:
  MockUserConsentHandler();
  ~MockUserConsentHandler() override;
  MOCK_METHOD(void,
              RequestUserConsent,
              (const std::string& one_time_code,
               CompletionCallback on_complete),
              (override));
  MOCK_METHOD(bool, is_active, (), (const, override));
  MOCK_METHOD(bool, is_async, (), (const, override));
};

}  // namespace content
#endif  // CONTENT_BROWSER_SMS_TEST_MOCK_USER_CONSENT_HANDLER_H_
