// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol SendTabToSelfModalPositioner;

// PresentationController for the modal dialog.
@interface SendTabToSelfModalPresentationController : UIPresentationController

// Delegate used to position the modal dialog.
@property(nonatomic, weak) id<SendTabToSelfModalPositioner> modalPositioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_PRESENTATION_CONTROLLER_H_
