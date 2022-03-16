// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_BLOCK_TYPES_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_BLOCK_TYPES_H_

#import <Foundation/Foundation.h>

// Completion callback for a request.
// |success| is YES if the operation was successful.
typedef void (^RequestCompletionBlock)(BOOL success);

// Block to call for unfollowing or refollowing a web channel.
// |completion| is called at completion of the request.
typedef void (^FollowRequestBlock)(RequestCompletionBlock completion);

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_BLOCK_TYPES_H_
