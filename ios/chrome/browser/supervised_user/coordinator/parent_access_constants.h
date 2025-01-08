// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Name of the script message handler that parses parent access callbacks.
extern NSString* const kParentAccessScriptMessageHandlerName;

// URL that hosts the parent approval widget.
NSURL* ParentAccessURL();

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_CONSTANTS_H_
