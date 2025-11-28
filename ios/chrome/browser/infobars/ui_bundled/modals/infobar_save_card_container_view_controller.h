// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarSaveCardModalConsumer;
@protocol InfobarSaveCardModalDelegate;

// InfobarSaveCardContainerViewController represents the container for the
// Save Card InfobarModal.
@interface InfobarSaveCardContainerViewController : UIViewController

// The consumer for this modal.
@property(nonatomic, readonly) id<InfobarSaveCardModalConsumer>
    saveCardConsumer;

// Instantiates with the `modalDelegate`.
- (instancetype)initWithModalDelegate:
    (id<InfobarSaveCardModalDelegate>)modalDelegate NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_CONTAINER_VIEW_CONTROLLER_H_
