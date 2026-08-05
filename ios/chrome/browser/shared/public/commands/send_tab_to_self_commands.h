// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEND_TAB_TO_SELF_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEND_TAB_TO_SELF_COMMANDS_H_

#import <Foundation/Foundation.h>

class GURL;
namespace send_tab_to_self {
enum class ShareEntryPoint;
}

// Commands to display Send Tab to Self UI.
@protocol SendTabToSelfCommands

// Displays the Send Tab to Self target device picker UI for `url` and `title`.
- (void)showSendTabToSelfUI:(const GURL&)url
                      title:(NSString*)title
                 entryPoint:(send_tab_to_self::ShareEntryPoint)entryPoint;

// Sends the tab with `url` and `title` directly to `deviceID` with `deviceName`
// without displaying the device picker UI.
- (void)sendTabToSelfToDeviceWithURL:(const GURL&)url
                               title:(NSString*)title
                            deviceID:(NSString*)deviceID
                          deviceName:(NSString*)deviceName
                          entryPoint:
                              (send_tab_to_self::ShareEntryPoint)entryPoint;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SEND_TAB_TO_SELF_COMMANDS_H_
