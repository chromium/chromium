// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// AdaptiveToolbarAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface AdaptiveToolbarAppInterface : NSObject

// Creates an infobar with `title`. Returns nil on success, or else an NSError
// indicating why the operation failed.
+ (BOOL)addInfobarWithTitle:(NSString*)title;

// Change the trait collection to compact width and returns the new trait
// collection.
+ (UITraitCollection*)changeTraitCollection:(UITraitCollection*)traitCollection
                          forViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_APP_INTERFACE_H_
