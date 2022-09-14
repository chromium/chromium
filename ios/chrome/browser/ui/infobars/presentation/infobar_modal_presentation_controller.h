// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarModalPositioner;

// PresentationController for the ModalInfobar.
@interface InfobarModalPresentationController : UIPresentationController

// Designated initializer. `modalPositioner` is used to position the
// ModalInfobar, it can't be nil.
- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
                    modalPositioner:(id<InfobarModalPositioner>)modalPositioner
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_CONTROLLER_H_
