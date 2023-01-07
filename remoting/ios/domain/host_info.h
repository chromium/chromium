// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DOMAIN_HOST_INFO_H_
#define REMOTING_IOS_DOMAIN_HOST_INFO_H_

#import <Foundation/Foundation.h>

namespace remoting {
namespace apis {
namespace v1 {
class HostInfo;
}  // namespace v1
}  // namespace apis
}  // namespace remoting

// A detail record for a Remoting Host.
@interface HostInfo : NSObject

// Various properties of the Remoting Host.
@property(nonatomic, copy) NSString* createdTime;
@property(nonatomic, copy) NSString* hostId;
@property(nonatomic, copy) NSString* hostName;
@property(nonatomic, copy) NSString* hostOs;
@property(nonatomic, copy) NSString* hostOsVersion;
@property(nonatomic, copy) NSString* hostVersion;
@property(nonatomic, copy) NSString* jabberId;
@property(nonatomic, copy) NSString* ftlId;
@property(nonatomic, copy) NSString* kind;
@property(nonatomic, copy) NSString* publicKey;
@property(nonatomic, copy) NSString* updatedTime;
@property(nonatomic, copy) NSString* offlineReason;
@property(nonatomic) BOOL isOnline;

- (instancetype)initWithRemotingHostInfo:
    (const remoting::apis::v1::HostInfo&)hostInfo;

@end

#endif  //  REMOTING_IOS_DOMAIN_HOST_INFO_H_
