// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_SERVICE_DEPRECATED_H_
#define IOS_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_SERVICE_DEPRECATED_H_

#import "ios/chrome/browser/mailto_handler/mailto_handler_service.h"

class ChromeBrowserState;

// Implementation of MailtoHandlerService based on MailtoHandlerProvider.
class MailtoHandlerServiceDeprecated : public MailtoHandlerService {
 public:
  explicit MailtoHandlerServiceDeprecated(ChromeBrowserState* browser_state);
  ~MailtoHandlerServiceDeprecated() override;

  // KeyedService implementation.
  void Shutdown() override;

  // MailtoHandlerService implementation.
  NSString* SettingsTitle() const override;
  UIViewController* CreateSettingsController() override;
  void DismissAllMailtoHandlerInterfaces() override;
  void HandleMailtoURL(NSURL* url) override;
};

#endif  // IOS_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_SERVICE_DEPRECATED_H_
