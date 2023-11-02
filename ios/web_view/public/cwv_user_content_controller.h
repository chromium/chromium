// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVUserScript;

// Allows injecting custom scripts into CWVWebView created with the
// configuration.
CWV_EXPORT
@interface CWVUserContentController : NSObject

// The user scripts associated with the configuration.
@property(nonatomic, copy, readonly) NSArray<CWVUserScript*>* userScripts;

- (instancetype)init NS_UNAVAILABLE;

// Adds a user script.
- (void)addUserScript:(CWVUserScript*)userScript;

// Removes all associated user scripts.
- (void)removeAllUserScripts;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_
