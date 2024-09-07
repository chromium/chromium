// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/domain/host_info.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "remoting/proto/remoting/v1/host_info.pb.h"

@implementation HostInfo

@synthesize createdTime = _createdTime;
@synthesize hostId = _hostId;
@synthesize hostName = _hostName;
@synthesize hostOs = _hostOs;
@synthesize hostOsVersion = _hostOsVersion;
@synthesize hostVersion = _hostVersion;
@synthesize jabberId = _jabberId;
@synthesize kind = _kind;
@synthesize publicKey = _publicKey;
@synthesize updatedTime = _updatedTime;
@synthesize offlineReason = _offlineReason;
@synthesize isOnline = _isOnline;

- (instancetype)initWithRemotingHostInfo:
    (const remoting::apis::v1::HostInfo&)hostInfo {
  if ((self = [super init])) {
    _hostId = base::SysUTF8ToNSString(hostInfo.host_id());
    _hostName = base::SysUTF8ToNSString(hostInfo.host_name());
    _hostOs = base::SysUTF8ToNSString(hostInfo.host_os_name());
    _hostOsVersion = base::SysUTF8ToNSString(hostInfo.host_os_version());
    _hostVersion = base::SysUTF8ToNSString(hostInfo.host_version());
    _jabberId = base::SysUTF8ToNSString(hostInfo.jabber_id());
    _ftlId = base::SysUTF8ToNSString(hostInfo.ftl_id());
    _publicKey = base::SysUTF8ToNSString(hostInfo.public_key());

    base::Time last_seen_time =
        base::Time::UnixEpoch() + base::Milliseconds(hostInfo.last_seen_time());
    _updatedTime = base::SysUTF16ToNSString(
        base::TimeFormatShortDateAndTime(last_seen_time));

    _offlineReason = base::SysUTF8ToNSString(hostInfo.host_offline_reason());
    _isOnline = hostInfo.status() == remoting::apis::v1::HostInfo_Status_ONLINE;
  }
  return self;
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"HostInfo: name=%@ online=%@ updatedTime= %@",
                                 _hostName, @(_isOnline), _updatedTime];
}

@end
