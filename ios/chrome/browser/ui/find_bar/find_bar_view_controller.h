// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class FindBarView;

@interface FindBarViewController : UIViewController

- (instancetype)initWithDarkAppearance:(BOOL)darkAppearance
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The FindBarView managed by this view controller. This is the same as the
// |view| property.
@property(nonatomic, strong, readonly) FindBarView* findBarView;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_VIEW_CONTROLLER_H_
