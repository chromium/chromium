// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_OVERSCROLL_ACTIONS_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_TEST_FAKES_FAKE_OVERSCROLL_ACTIONS_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_view.h"

// Fake OverscrollActionsControllerDelegate used for testing.
// The delegate saves the last triggered action, and provide a view to be used
// as a parent to the OverscrollActionsView.
@interface FakeOverscrollActionsControllerDelegate
    : NSObject <OverscrollActionsControllerDelegate>

// The OverscrollAction parameter that was used to call
// |overscrollActionsController:didTriggerAction:| with.
@property(nonatomic, assign) OverscrollAction selectedAction;

// The header view, acts as the superview for overscrollActionsView.
@property(nonatomic, strong) UIView* headerView;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_OVERSCROLL_ACTIONS_CONTROLLER_DELEGATE_H_
