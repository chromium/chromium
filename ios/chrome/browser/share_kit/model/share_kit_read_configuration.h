// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_READ_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_READ_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "base/types/expected.h"
#import "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#import "third_party/abseil-cpp/absl/status/status.h"

// The configuration to read a single group.
@interface ShareKitReadGroupParamConfiguration : NSObject

// The collab ID of the group to be read, also sometimes called groupID.
@property(nonatomic, copy) NSString* collabID;

// As an optimization, sending a consistency token will allow for reading groups
// with bounded staleness. Without a consistency token, we'll always read the
// most current group.
@property(nonatomic, copy) NSString* consistencyToken;

@end

// Configuration object for reading shared groups.
@interface ShareKitReadGroupsConfiguration : NSObject

// The parameters for the groups to be read.
@property(nonatomic, copy)
    NSArray<ShareKitReadGroupParamConfiguration*>* groupsParam;

// The callback once the groups have been read.
@property(nonatomic, copy) void (^callback)
    (const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&);

@end

// Configuration object for reading a single shared group with a token secret.
@interface ShareKitReadGroupWithTokenConfiguration : NSObject

// The collab ID of the group to be read, also sometimes called groupID.
@property(nonatomic, copy) NSString* collabID;

// As an optimization, sending a consistency token will allow for reading groups
// with bounded staleness. Without a consistency token, we'll always read the
// most current group.
@property(nonatomic, copy) NSString* consistencyToken;

// Token secret is used to grant the requester access to something they wouldn't
// otherwise have access to. For example, when reading a group before the
// requester has joined it. This token secret is retrieved from the group
// invitation link. A token secret is specific to a group.
@property(nonatomic, copy) NSString* tokenSecret;

// The callback once the groups have been read.
@property(nonatomic, copy) void (^callback)
    (const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_READ_CONFIGURATION_H_
