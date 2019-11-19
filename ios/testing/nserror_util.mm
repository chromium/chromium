// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/nserror_util.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A custom NSError subclass that is marked as an eDO "value type", allowing
// it to be serialized and reconstructed in the remote process, rather than
// having all its method proxied via IPC.
@interface ChromeRemoteError : NSError
@end

@implementation ChromeRemoteError
- (BOOL)edo_isEDOValueType {
  return YES;
}
@end

namespace testing {

NSError* NSErrorWithLocalizedDescription(NSString* error_description) {
  NSString* errorDomain = @"com.google.chrome.errorDomain";
  NSDictionary* userInfo = @{
    NSLocalizedDescriptionKey : error_description,
  };

  return [[ChromeRemoteError alloc] initWithDomain:errorDomain
                                              code:0
                                          userInfo:userInfo];
}

}  // namespace testing
