// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_COMMON_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_COMMON_TAB_HELPER_DELEGATE_H_

#import "ios/chrome/browser/passwords/password_controller_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"

// Protocol containing all of the tab helper delegate protocols needed to set
// up webstates after the UI is available.
// This protocol is scaffolding for refactoring these delegate responsibilities
// out of the BVC. The goal is to reduce the number of these delegate protocols
// that the BVC conforms to to zero.
@protocol CommonTabHelperDelegate <BubblePresenterDelegate,
                                   OverscrollActionsControllerDelegate,
                                   // TODO(crbug.com/1272487): Factor
                                   // PasswordControllerDelegate out of the BVC.
                                   PasswordControllerDelegate>
@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_COMMON_TAB_HELPER_DELEGATE_H_
