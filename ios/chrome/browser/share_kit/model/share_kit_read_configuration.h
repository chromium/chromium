// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_READ_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_READ_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "base/types/expected.h"
#import "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#import "third_party/abseil-cpp/absl/status/status.h"

// Configuration object for reading a shared group.
@interface ShareKitReadConfiguration : NSObject

// The list of collabs ID to be read.
@property(nonatomic, copy) NSArray<NSString*>* collabIDs;

// The callback once the groups have been read.
@property(nonatomic, copy) void (^callback)
    (const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_READ_CONFIGURATION_H_
