// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_INFOBARS_SC_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_INFOBARS_SC_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"

@class InfobarBannerTransitionDriver;
@class InfobarBannerViewController;

@interface ContainerViewController
    : UIViewController <InfobarBannerPositioner, InfobarModalPositioner>
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
@end

#endif  // IOS_SHOWCASE_INFOBARS_SC_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
