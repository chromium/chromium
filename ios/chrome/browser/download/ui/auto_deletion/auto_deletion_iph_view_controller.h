// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_IPH_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_IPH_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol AutoDeletionMutator;
class Browser;

// The UIViewController that manages the Auto-deletion IPH's view.
@interface AutoDeletionIPHViewController : UIViewController

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<AutoDeletionMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_IPH_VIEW_CONTROLLER_H_
