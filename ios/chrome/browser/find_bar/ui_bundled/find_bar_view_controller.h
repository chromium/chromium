// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class FindBarView;

@protocol FindBarViewControllerDelegate
// Called to dismiss the find bar.
- (void)dismiss;
@end

@interface FindBarViewController : UIViewController

- (instancetype)initWithDarkAppearance:(BOOL)darkAppearance
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The FindBarView managed by this view controller. This is the same as the
// `view` property.
@property(nonatomic, strong, readonly) FindBarView* findBarView;
// The delegate is called to dismiss the find bar.
@property(nonatomic, weak) id<FindBarViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_VIEW_CONTROLLER_H_
