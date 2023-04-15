// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_APP_INTERFACE_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test methods that perform actions on Web Shell. These methods may read or
// alter Web Shell's internal state programmatically or via the UI, but in both
// cases will properly synchronize the UI for Earl Grey tests.
@interface ShellEarlGreyAppInterface : NSObject

// Loads `spec` as an URL in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)spec;

// Returns YES if the current WebState is loading.
+ (BOOL)isCurrentWebStateLoading [[nodiscard]];

// Returns YES if the current WebState contains the given `text`.
+ (BOOL)currentWebStateContainsText:(NSString*)text [[nodiscard]];

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_EARL_GREY_APP_INTERFACE_H_
