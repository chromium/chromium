// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_SWITCHING_ACCOUNT_SWITCHER_TRANSITION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_SWITCHING_ACCOUNT_SWITCHER_TRANSITION_DELEGATE_H_

#import <UIKit/UIKit.h>

// Transition Delegate for the AccountSwitcher. It is presenting it as a modal.
@interface AccountSwitcherTransitionDelegate
    : NSObject <UIViewControllerTransitioningDelegate>

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_SWITCHING_ACCOUNT_SWITCHER_TRANSITION_DELEGATE_H_
