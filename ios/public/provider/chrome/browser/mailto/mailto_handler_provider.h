// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_MAILTO_HANDLER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_MAILTO_HANDLER_PROVIDER_H_

#import <UIKit/UIKit.h>

class ChromeBrowserState;
@class ChromeIdentity;

typedef ChromeIdentity* (^SignedInIdentityBlock)(void);
typedef NSArray<ChromeIdentity*>* (^SignedInIdentitiesBlock)(void);

// An provider to handle the opening of mailto links.
class MailtoHandlerProvider {
 public:
  MailtoHandlerProvider();

  MailtoHandlerProvider(const MailtoHandlerProvider&) = delete;
  MailtoHandlerProvider& operator=(const MailtoHandlerProvider&) = delete;

  virtual ~MailtoHandlerProvider();

  // Sets up mailto handling for |browser_state|.
  virtual void PrepareMailtoHandling(ChromeBrowserState* browser_state);

  // Unregisters the mailto handler for browser state.
  virtual void RemoveMailtoHandling();

  // Returns a properly localized title for the menu item or button used to open
  // the settings for this handler. Returns nil if mailto handling is not
  // supported by the provider.
  virtual NSString* MailtoHandlerSettingsTitle() const;

  // Creates and returns a view controller for presenting the settings for
  // mailto handling to the user. Returns nil if mailto handling is not
  // supported by the provider.
  virtual UIViewController* MailtoHandlerSettingsController() const;

  // Dismisses any mailto handling UI immediately. Handling is cancelled.
  virtual void DismissAllMailtoHandlerInterfaces() const;

  // Handles the specified mailto: URL. The provider falls back on the built-in
  // URL handling in case of error.
  virtual void HandleMailtoURL(NSURL* url) const;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_MAILTO_HANDLER_PROVIDER_H_
