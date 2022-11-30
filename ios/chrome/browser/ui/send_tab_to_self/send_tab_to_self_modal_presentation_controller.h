// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarModalPositioner;

// PresentationController for the modal dialog.
@interface SendTabToSelfModalPresentationController : UIPresentationController

// Delegate used to position the modal dialog.
@property(nonatomic, weak) id<InfobarModalPositioner> modalPositioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_PRESENTATION_CONTROLLER_H_
