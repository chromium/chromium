// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PICTURE_IN_PICTURE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PICTURE_IN_PICTURE_COMMANDS_H_

#import <UIKit/UIKit.h>

@class PictureInPictureConfiguration;

// Protocol for Picture-in-Picture commands.
@protocol PictureInPictureCommands <NSObject>

// Shows picture-in-picture with the given configuration.
- (void)showPictureInPictureWithConfig:(PictureInPictureConfiguration*)config;

// Dismisses picture-in-picture.
- (void)dismissPictureInPicture;

// Command picture in picture to be dismissed if app was not restored from
// picture-in-picture restore action.
- (void)dismissPictureInPictureIfNotPipRestore;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PICTURE_IN_PICTURE_COMMANDS_H_
