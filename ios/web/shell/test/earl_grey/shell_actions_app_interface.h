// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_ACTIONS_APP_INTERFACE_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_ACTIONS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@class ElementSelector;
@protocol GREYAction;

// Helpers that return GREYActions which are useful for testing ios_web_shell.
// These helpers are compiled into the app binary and can be called from either
// app or test code.
@interface ShellActionsAppInterface : NSObject

// Action to longpress on the element found by `selector` in the shell's
// webview.  This gesture is expected to cause the context menu to appear, and
// is not expected to trigger events in the webview. This action doesn't fail if
// the context menu isn't displayed; calling code should check for that
// separately with a matcher.
+ (id<GREYAction>)longPressElementForContextMenu:(ElementSelector*)selector;

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_SHELL_ACTIONS_APP_INTERFACE_H_
