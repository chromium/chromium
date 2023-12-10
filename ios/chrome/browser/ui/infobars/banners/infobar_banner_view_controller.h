// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_interaction_delegate.h"

@protocol InfobarBannerDelegate;

// ViewController that manages an InfobarBanner. It consists of a leading icon,
// a title and optional subtitle, and a trailing button.
@interface InfobarBannerViewController
    : UIViewController <InfobarBannerConsumer, InfobarBannerInteractable>

// Designated Initializer. `delegate` handles InfobarBannerVC actions.
// `presentsModal` should be YES if the banner is able to present an
// InfobarModal. `infobarType` is used to know which Coordinator presented this
// VC.
- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate
                   presentsModal:(BOOL)presentsModal
                            type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// - If no interaction is occuring, the InfobarBanner will be dismissed.
// - If there's some interaction occuring the InfobarBanner will be dismissed
// once this interaction ends.
// - If the InfobarBanner was dismissed or is now presenting an InfobarModal
// because of the last interaction. This method will NO-OP.
- (void)dismissWhenInteractionIsFinished;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_
