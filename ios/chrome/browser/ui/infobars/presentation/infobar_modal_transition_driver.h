// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_TRANSITION_DRIVER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_TRANSITION_DRIVER_H_

#import <UIKit/UIKit.h>

@protocol InfobarModalPositioner;

typedef NS_ENUM(NSInteger, InfobarModalTransition) {
  // InfobarModal will be presented from a base ViewController.
  InfobarModalTransitionBase,
  // InfobarModal will be presented by an InfobarBanner ViewController.
  InfobarModalTransitionBanner,
};

// The transition delegate used to present an InfobarModal.
@interface InfobarModalTransitionDriver
    : NSObject <UIViewControllerTransitioningDelegate>

- (instancetype)initWithTransitionMode:(InfobarModalTransition)transitionMode
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The InfobarModalTransition mode being used for this Transition driver.
@property(nonatomic, assign, readonly) InfobarModalTransition transitionMode;

// Delegate used to position the ModalInfobar.
@property(nonatomic, weak) id<InfobarModalPositioner> modalPositioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_TRANSITION_DRIVER_H_
