// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_PROTECTED_H_

#import "ios/chrome/browser/infobars/infobar_controller.h"

@interface InfoBarController ()

// Returns a view with all the infobar elements in it. Will not add it as a
// subview yet. This method must be overriden in subclasses.
- (UIView*)infobarView;

// Returns whether user interaction with the infobar should be ignored.
- (BOOL)shouldIgnoreUserInteraction;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_PROTECTED_H_
