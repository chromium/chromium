// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarModalDelegate;

// UIViewController for a Modal Infobar. It contains more information and
// options than its corresponding Banner Infobar.
@interface InfobarModalViewController : UIViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarModalDelegate>)infobarModalDelegate NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_VIEW_CONTROLLER_H_
