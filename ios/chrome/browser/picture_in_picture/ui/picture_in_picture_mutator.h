// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_MUTATOR_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_MUTATOR_H_

#import <Foundation/Foundation.h>

// Protocol for Picture in Picture mutator.
@protocol PictureInPictureMutator <NSObject>

// Starts picture in picture.
- (void)startDestination;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_MUTATOR_H_
