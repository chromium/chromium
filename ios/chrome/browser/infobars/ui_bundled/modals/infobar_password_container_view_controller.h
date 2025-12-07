// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_PASSWORD_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_PASSWORD_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarPasswordModalConsumer;
@protocol InfobarPasswordModalDelegate;
enum class InfobarType;

// InfobarPasswordContainerViewController represents the container for the
// Passwords InfobarModal.
@interface InfobarPasswordContainerViewController : UIViewController

// The consumer for this modal.
@property(nonatomic, readonly) id<InfobarPasswordModalConsumer>
    passwordConsumer;

// Initializes with a `modalDelegate` and a `type`.
- (instancetype)initWithDelegate:(id<InfobarPasswordModalDelegate>)modalDelegate
                            type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_PASSWORD_CONTAINER_VIEW_CONTROLLER_H_
