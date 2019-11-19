// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_JS_FINDINPAGE_MANAGER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_JS_FINDINPAGE_MANAGER_H_

#include <CoreGraphics/CGBase.h>
#include <CoreGraphics/CGGeometry.h>

#include "base/ios/block_types.h"
#import "ios/web/public/deprecated/crw_js_injection_manager.h"

// Data from find in page.
typedef struct FindInPageEntry {
  CGPoint point;    // Scroll offset required to center the highlighted item.
  NSInteger index;  // Currently higlighted search term.
} FindInPageEntry;

// Constant for "not found".
extern FindInPageEntry FindInPageEntryZero;

@class CRWJSInjectionReceiver;
@class FindInPageModel;

// Manager for the injection of the Find In Page JavaScript.
@interface JsFindinpageManager : CRWJSInjectionManager

// Find In Page model.
@property(nonatomic, readwrite, strong) FindInPageModel* findInPageModel;

// Sets the width and height of the window.
- (void)setWidth:(CGFloat)width height:(CGFloat)height;

// Runs injected JavaScript to find |query| string. Calls |completionHandler|
// with YES if the find operation completed, it is called with NO otherwise.
// If the find operation was successfiul the first match to scroll to is
// also called with. If the |completionHandler| is called with NO, another
// call to |pumpWithCompletionHandler:| is required. |completionHandler| cannot
// be nil.
- (void)findString:(NSString*)query
    completionHandler:(void (^)(BOOL, CGPoint))completionHandler;

// Searches for more matches. Calls |completionHandler| with a success BOOL and
// scroll position if pumping was successful. If the pumping was unsuccessful
// another pumping call maybe required. |completionHandler| cannot be nil.
- (void)pumpWithCompletionHandler:(void (^)(BOOL, CGPoint))completionHandler;

// Moves to the next matched location and executes the completion handler with
// the new scroll position passed in. The |completionHandler| can be nil.
- (void)nextMatchWithCompletionHandler:(void (^)(CGPoint))completionHandler;

// Moves to the previous matched location and executes the completion handle
// with the new scroll position passed in. The |completionHandler| can be nil.
- (void)previousMatchWithCompletionHandler:(void (^)(CGPoint))completionHandler;

// Stops find in page and calls |completionHandler| once find in page is
// stopped. |completionHandler| cannot be nil.
- (void)disableWithCompletionHandler:(ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_JS_FINDINPAGE_MANAGER_H_
