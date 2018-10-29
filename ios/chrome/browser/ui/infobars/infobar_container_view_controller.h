// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/infobars/infobar_container_state_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"

@protocol InfobarPositioner;

// ViewController that contains all Infobars. It can contain various at the
// same time but only the top most one will be visible.
@interface InfobarContainerViewController
    : UIViewController<InfobarContainerConsumer, InfobarContainerStateDelegate>

// The delegate used to position the InfoBarContainer in the view.
@property(nonatomic, weak) id<InfobarPositioner> positioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
