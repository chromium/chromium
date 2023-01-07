// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_EARL_GREY_SCOPED_BLOCK_SWIZZLER_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_EARL_GREY_SCOPED_BLOCK_SWIZZLER_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// EarlGreyScopedBlockSwizzlerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface EarlGreyScopedBlockSwizzlerAppInterface : NSObject

// Creates and retains a ScopedBlockSwizzler.  Returns a unique id to be used
// to delete that ScopedBlockSwizzler.
+ (int)createScopedBlockSwizzlerForTarget:(NSString*)target
                             withSelector:(NSString*)selector
                                withBlock:(id)block;

// Deletes a ScopedBlockSwizzler based on it's unique id.
+ (void)deleteScopedBlockSwizzlerForID:(int)uniqueID;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_EARL_GREY_SCOPED_BLOCK_SWIZZLER_APP_INTERFACE_H_
