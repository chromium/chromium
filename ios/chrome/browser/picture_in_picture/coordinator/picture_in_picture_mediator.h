// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_COORDINATOR_PICTURE_IN_PICTURE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_COORDINATOR_PICTURE_IN_PICTURE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_mutator.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"

@class PictureInPictureConfiguration;

// Mediator for picture in picture.
@interface PictureInPictureMediator
    : NSObject <ButtonStackActionDelegate, PictureInPictureMutator>

// Initializes the mediator with the given configuration.
- (instancetype)initWithConfiguration:
    (PictureInPictureConfiguration*)configuration NS_DESIGNATED_INITIALIZER;

// Unavailable initializer.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_COORDINATOR_PICTURE_IN_PICTURE_MEDIATOR_H_
