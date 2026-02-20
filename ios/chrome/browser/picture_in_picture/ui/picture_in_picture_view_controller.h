// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@class PictureInPictureConfiguration;
@protocol PictureInPictureMutator;
@protocol PictureInPictureCommands;

// View controller for picture in picture.
@interface PictureInPictureViewController : ButtonStackViewController

// Mutator for picture in picture.
@property(nonatomic, weak) id<PictureInPictureMutator> mutator;

// Handler for the picture in picture commands.
@property(nonatomic, weak) id<PictureInPictureCommands> handler;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
           primaryButtonTitle:(NSString*)primaryButtonTitle
                     videoURL:(NSURL*)videoURL NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

// Unavailable initializer.
- (instancetype)init NS_UNAVAILABLE;

// Dismisses picture in picture if the user returned to the app manually instead
// of using the picture in picture restore action.
- (void)dismissIfNotPipRestore;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_VIEW_CONTROLLER_H_
