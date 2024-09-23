// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_INFO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_INFO_COMMANDS_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Commands related to the Page Info UI.
@protocol PageInfoCommands

// Shows the page security info.
- (void)showPageInfo;

// Hides the page security info.
- (void)hidePageInfo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_INFO_COMMANDS_H_
