// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_RESPONSE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_RESPONSE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Contains information about a navigation response, used for making policy
// decisions.
CWV_EXPORT
@interface CWVNavigationResponse : NSObject

// A Boolean value indicating whether the frame being navigated is the main
// frame.
@property(nonatomic, readonly, getter=isForMainFrame) BOOL forMainFrame;

// The frame's response.
@property(nonatomic, readonly, copy) NSURLResponse* response;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_RESPONSE_H_
