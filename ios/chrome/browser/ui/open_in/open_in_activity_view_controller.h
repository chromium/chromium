// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_ACTIVITY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_ACTIVITY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol OpenInActivityDelegate;

// View controller that provides an interface to perform actions on a file.
@interface OpenInActivityViewController : UIActivityViewController

// Initializes an UIActivityViewController with the given `fileURL`.
- (instancetype)initWithURL:(NSURL*)fileURL NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithActivityItems:(NSArray*)activityItems
                applicationActivities:
                    (NSArray<__kindof UIActivity*>*)applicationActivities
    NS_UNAVAILABLE;

// Delegate used to handle presentation actions.
@property(nonatomic, weak) id<OpenInActivityDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_ACTIVITY_VIEW_CONTROLLER_H_
