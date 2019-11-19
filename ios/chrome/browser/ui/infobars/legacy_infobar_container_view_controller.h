// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_LEGACY_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_LEGACY_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"

class FullscreenController;

@protocol InfobarPositioner;

// ViewController that contains all Infobars. It can contain various at the
// same time but only the top most one will be visible.
@interface LegacyInfobarContainerViewController
    : UIViewController <InfobarContainerConsumer>

// |fullscreenController| must not be nullptr.
- (instancetype)initWithFullscreenController:
    (FullscreenController*)fullscreenController NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// The delegate used to position the InfoBarContainer in the view.
@property(nonatomic, weak) id<InfobarPositioner> positioner;

// If YES the Container Fullscreen support will be disabled.
@property(nonatomic, assign) BOOL disableFullscreenSupport;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_LEGACY_INFOBAR_CONTAINER_VIEW_CONTROLLER_H_
