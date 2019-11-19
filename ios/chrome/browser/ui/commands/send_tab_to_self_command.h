// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SEND_TAB_TO_SELF_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SEND_TAB_TO_SELF_COMMAND_H_

#import <Foundation/Foundation.h>

@interface SendTabToSelfCommand : NSObject

@property(copy, nonatomic, readonly) NSString* targetDeviceID;
@property(copy, nonatomic, readonly) NSString* targetDeviceName;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithTargetDeviceID:(NSString*)targetDeviceID
                      targetDeviceName:(NSString*)targetDeviceName
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SEND_TAB_TO_SELF_COMMAND_H_