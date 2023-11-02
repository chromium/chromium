// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/domain/client_session_details.h"

#import "remoting/ios/domain/host_info.h"

@implementation ClientSessionDetails

@synthesize hostInfo = _hostInfo;
@synthesize state = _state;
@synthesize error = _error;

- (NSString*)description {
  return
      [NSString stringWithFormat:@"ClientSessionDetails: state=%d hostInfo=%@",
                                 (int)_state, _hostInfo];
}

@end
