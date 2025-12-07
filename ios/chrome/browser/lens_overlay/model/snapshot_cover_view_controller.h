// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_SNAPSHOT_COVER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_SNAPSHOT_COVER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Displays an image covering the entire width and height of the view.
@interface SnapshotCoverViewController : UIViewController

// Creates a new instance with the given image and an action to be run when
// the view controller first appears.
- (instancetype)initWithImage:(UIImage*)image
                onFirstAppear:(ProceduralBlock)onAppear
    NS_DESIGNATED_INITIALIZER;

// Creates a new instance with the given image.
- (instancetype)initWithImage:(UIImage*)image NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_SNAPSHOT_COVER_VIEW_CONTROLLER_H_
