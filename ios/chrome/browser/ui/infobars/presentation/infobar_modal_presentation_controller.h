// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarModalPositioner;

// PresentationController for the ModalInfobar.
@interface InfobarModalPresentationController : UIPresentationController

// Delegate used to position the ModalInfobar.
@property(nonatomic, assign) id<InfobarModalPositioner> modalPositioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_CONTROLLER_H_
