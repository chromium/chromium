// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mailto_handler/mailto_handler_service_deprecated.h"

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

MailtoHandlerServiceDeprecated::MailtoHandlerServiceDeprecated(
    ChromeBrowserState* browser_state) {
  ios::GetChromeBrowserProvider()
      .GetMailtoHandlerProvider()
      ->PrepareMailtoHandling(browser_state);
}

MailtoHandlerServiceDeprecated::~MailtoHandlerServiceDeprecated() {}

void MailtoHandlerServiceDeprecated::Shutdown() {
  ios::GetChromeBrowserProvider()
      .GetMailtoHandlerProvider()
      ->RemoveMailtoHandling();
}

NSString* MailtoHandlerServiceDeprecated::SettingsTitle() const {
  return ios::GetChromeBrowserProvider()
      .GetMailtoHandlerProvider()
      ->MailtoHandlerSettingsTitle();
}

UIViewController* MailtoHandlerServiceDeprecated::CreateSettingsController() {
  return ios::GetChromeBrowserProvider()
      .GetMailtoHandlerProvider()
      ->MailtoHandlerSettingsController();
}

void MailtoHandlerServiceDeprecated::DismissAllMailtoHandlerInterfaces() {
  ios::GetChromeBrowserProvider()
      .GetMailtoHandlerProvider()
      ->DismissAllMailtoHandlerInterfaces();
}

void MailtoHandlerServiceDeprecated::HandleMailtoURL(NSURL* url) {
  ios::GetChromeBrowserProvider().GetMailtoHandlerProvider()->HandleMailtoURL(
      url);
}
