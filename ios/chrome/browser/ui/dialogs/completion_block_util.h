// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_COMPLETION_BLOCK_UTIL_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_COMPLETION_BLOCK_UTIL_H_

#import <Foundation/Foundation.h>

namespace completion_block_util {

// Typedefs are required to return Objective C blocks from C++ util functions.
typedef void (^AlertCallback)(void);
typedef void (^ConfirmCallback)(BOOL isConfirmed);
typedef void (^PromptCallback)(NSString* input);
typedef void (^HTTPAuthCallack)(NSString* user, NSString* password);
typedef void (^DecidePolicyCallback)(BOOL shouldContinue);

// Completion callbacks provided by web// for dialogs have a built-in
// mechanism that throws an exception if they are deallocated before being
// executed.  The following utility functions convert these web// completion
// blocks into safer versions that ensure that the original block is called
// before being deallocated.
AlertCallback GetSafeJavaScriptAlertCompletion(AlertCallback callback);
ConfirmCallback GetSafeJavaScriptConfirmationCompletion(
    ConfirmCallback callback);
PromptCallback GetSafeJavaScriptPromptCompletion(PromptCallback callback);
HTTPAuthCallack GetSafeHTTPAuthCompletion(HTTPAuthCallack callback);
DecidePolicyCallback GetSafeDecidePolicyCompletion(
    DecidePolicyCallback callback);

}  // completion_block_util

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_COMPLETION_BLOCK_UTIL_H_
