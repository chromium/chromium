// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_LEAVE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_LEAVE_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "third_party/abseil-cpp/absl/status/status.h"

// The configuration to leave a group.
@interface ShareKitLeaveConfiguration : NSObject

// The collabID of the group to be left.
@property(nonatomic, copy) NSString* collabID;

// The callback when the leave is complete.
@property(nonatomic, strong) void (^callback)(const absl::Status&);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_LEAVE_CONFIGURATION_H_
