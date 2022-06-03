// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

MailtoHandlerProvider::MailtoHandlerProvider() {}

MailtoHandlerProvider::~MailtoHandlerProvider() {}

void MailtoHandlerProvider::PrepareMailtoHandling(
    ChromeBrowserState* browser_state) {}

void MailtoHandlerProvider::RemoveMailtoHandling() {}

NSString* MailtoHandlerProvider::MailtoHandlerSettingsTitle() const {
  return nil;
}

UIViewController* MailtoHandlerProvider::MailtoHandlerSettingsController()
    const {
  return nil;
}

void MailtoHandlerProvider::DismissAllMailtoHandlerInterfaces() const {}

void MailtoHandlerProvider::HandleMailtoURL(NSURL* url) const {
  [[UIApplication sharedApplication] openURL:url
                                     options:@{}
                           completionHandler:nil];
}
