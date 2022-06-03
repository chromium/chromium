// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_TEST_MAILTO_HANDLER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_TEST_MAILTO_HANDLER_PROVIDER_H_

#import "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"

// An provider to handle the opening of mailto links.
class TestMailtoHandlerProvider : public MailtoHandlerProvider {
 public:
  TestMailtoHandlerProvider();

  TestMailtoHandlerProvider(const TestMailtoHandlerProvider&) = delete;
  TestMailtoHandlerProvider& operator=(const TestMailtoHandlerProvider&) =
      delete;

  ~TestMailtoHandlerProvider() override;

  NSString* MailtoHandlerSettingsTitle() const override;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_TEST_MAILTO_HANDLER_PROVIDER_H_
