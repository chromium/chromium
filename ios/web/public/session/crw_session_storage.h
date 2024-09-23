// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_

#import <Foundation/Foundation.h>

#include "base/time/time.h"
#include "ios/web/common/user_agent.h"

namespace web {
class WebStateID;
namespace proto {
class WebStateMetadataStorage;
class WebStateStorage;
}  // namespace proto
}  // namespace web

@class CRWNavigationItemStorage;
@class CRWSessionUserData;
@class CRWSessionCertificatePolicyCacheStorage;

// NSCoding-compliant class used to serialize session state.
// TODO(crbug.com/40503548): Investigate using code from the sessions component.
@interface CRWSessionStorage : NSObject <NSCoding>

@property(nonatomic, assign) BOOL hasOpener;
@property(nonatomic, assign) NSInteger lastCommittedItemIndex;
@property(nonatomic, copy) NSArray<CRWNavigationItemStorage*>* itemStorages;
@property(nonatomic, strong)
    CRWSessionCertificatePolicyCacheStorage* certPolicyCacheStorage;
@property(nonatomic, strong) CRWSessionUserData* userData;
@property(nonatomic, assign) web::UserAgentType userAgentType;
@property(nonatomic, copy) NSString* stableIdentifier;
@property(nonatomic, assign) web::WebStateID uniqueIdentifier;
@property(nonatomic, assign) base::Time lastActiveTime;
@property(nonatomic, assign) base::Time creationTime;

// Convenience initializer that creates an instance from proto representation
// and identifiers.
- (instancetype)initWithProto:(const web::proto::WebStateStorage&)storage
             uniqueIdentifier:(web::WebStateID)uniqueIdentifier
             stableIdentifier:(NSString*)stableIdentifier;

// Serializes the CRWSessionStorage into `storage`.
- (void)serializeToProto:(web::proto::WebStateStorage&)storage;

// Serializes the metadata part of the CRWSessionStorage into `metadata`.
- (void)serializeMetadataToProto:(web::proto::WebStateMetadataStorage&)metadata;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_
