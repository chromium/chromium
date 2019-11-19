// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_password_infobar_controller.h"

#import "ios/chrome/browser/infobars/confirm_infobar_controller+protected.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation IOSPasswordInfoBarController

- (void)updateInfobarLabel:(ConfirmInfoBarView*)view {
  [super updateInfobarLabel:view];

  auto* delegate = static_cast<IOSChromePasswordManagerInfoBarDelegate*>(
      self.infoBarDelegate);
  NSString* message = delegate->GetDetailsMessageText();
  if (!message.length)
    return;

  [view addFooterLabel:message];
}

@end
