// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_TEST_MOCK_SMS_PROVIDER_H_
#define CONTENT_BROWSER_SMS_TEST_MOCK_SMS_PROVIDER_H_

#include "content/browser/sms/sms_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSmsProvider : public SmsProvider {
 public:
  MockSmsProvider();

  MockSmsProvider(const MockSmsProvider&) = delete;
  MockSmsProvider& operator=(const MockSmsProvider&) = delete;

  ~MockSmsProvider() override;

  MOCK_METHOD2(Retrieve, void(RenderFrameHost*, SmsFetchType));
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_TEST_MOCK_SMS_PROVIDER_H_
