// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_

#import <Foundation/Foundation.h>

#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "ios/web/common/user_agent.h"

@class CRWNavigationItemStorage;
@class CRWSessionUserData;
@class CRWSessionCertificatePolicyCacheStorage;

// NSCoding-compliant class used to serialize session state.
// TODO(crbug.com/685388): Investigate using code from the sessions component.
@interface CRWSessionStorage : NSObject <NSCoding>

@property(nonatomic, assign) BOOL hasOpener;
@property(nonatomic, assign) NSInteger lastCommittedItemIndex;
@property(nonatomic, copy) NSArray<CRWNavigationItemStorage*>* itemStorages;
@property(nonatomic, strong)
    CRWSessionCertificatePolicyCacheStorage* certPolicyCacheStorage;
@property(nonatomic, strong) CRWSessionUserData* userData;
@property(nonatomic, assign) web::UserAgentType userAgentType;
@property(nonatomic, copy) NSString* stableIdentifier;
@property(nonatomic, assign) SessionID uniqueIdentifier;
@property(nonatomic, assign) base::Time lastActiveTime;
@property(nonatomic, assign) base::Time creationTime;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_SESSION_STORAGE_H_
