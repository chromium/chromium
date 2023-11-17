// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_H_
#define IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "components/keyed_service/core/keyed_service.h"

// Service responsible for providing an handler for mailto: links.
class MailtoHandlerService : public KeyedService {
 public:
  MailtoHandlerService();
  ~MailtoHandlerService() override;

  // Returns a properly localized title for the menu item or button used to
  // open the settings for this handler. Returns nil if mailto: handling is
  // not supported.
  virtual NSString* SettingsTitle() const = 0;

  // Creates and returns a view controller for presenting the settings for
  // mailto: handling to the user. Returns nil if mailto: handling is not
  // supported.
  virtual UIViewController* CreateSettingsController() = 0;

  // Dismisses any mailto: handling UI immediately. Handling is cancelled.
  virtual void DismissAllMailtoHandlerInterfaces() = 0;

  // Handles the specified mailto: URL. Should fall back on the built-in
  // URL handling in case of error.
  virtual void HandleMailtoURL(NSURL* url, base::OnceClosure completion) = 0;
};

#endif  // IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_H_
