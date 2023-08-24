// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_FIND_IN_PAGE_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_FIND_IN_PAGE_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVFindInPageManager;

// Object that manages presentation of `UIFindInteraction` for iOS 16+.
CWV_EXPORT
@interface CWVFindInPageController : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Whether a Find in Page session can begin for the current web page.
- (BOOL)canFindInPage;

// Displays the Native `UIFindInteraction` FiP interface for query entry.
- (void)startFindInPage;

// Start a Find operation on the web state in a `UIFindInteraction`
// with the given `query`. Check `canFindInPage` before calling
// `findStringInPage`.
- (void)findStringInPage:(NSString*)query;

// Selects and scrolls to the next result if there is one. Otherwise, nothing
// will change. Loop back to the first result if currently on last result.
- (void)findNextStringInPage;

// Selects and scrolls to the previous result if there is one. Otherwise,
// nothing will change. Loop back to the first result if currently on last
// result.
- (void)findPreviousStringInPage;

// Stop any ongoing Find session. If called before `findStringInPage` nothing
// occurs.
- (void)stopFindInPage;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_FIND_IN_PAGE_CONTROLLER_H_
