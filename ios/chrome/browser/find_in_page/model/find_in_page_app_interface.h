// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Contains the app-side implementation of helpers for Native Find in Page. For
// JavaScript Find in Page, see JavaScriptFindInPageControllerAppInterface.
@interface FindInPageAppInterface : NSObject

// Clears the search term in FindInPageController.
+ (void)clearSearchTerm;

@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_APP_INTERFACE_H_
