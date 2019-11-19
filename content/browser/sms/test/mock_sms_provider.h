// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_TEST_MOCK_SMS_PROVIDER_H_
#define CONTENT_BROWSER_SMS_TEST_MOCK_SMS_PROVIDER_H_

#include "base/macros.h"
#include "content/browser/sms/sms_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSmsProvider : public SmsProvider {
 public:
  MockSmsProvider();
  ~MockSmsProvider() override;

  MOCK_METHOD0(Retrieve, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_TEST_MOCK_SMS_PROVIDER_H_
