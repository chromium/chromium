// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@class PictureInPictureConfiguration;
@protocol PictureInPictureMutator;

// View controller for picture in picture.
@interface PictureInPictureViewController : ButtonStackViewController

// Mutator for picture in picture.
@property(nonatomic, weak) id<PictureInPictureMutator> mutator;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
           primaryButtonTitle:(NSString*)primaryButtonTitle
                     videoURL:(NSURL*)videoURL NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

// Unavailable initializer.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_VIEW_CONTROLLER_H_
