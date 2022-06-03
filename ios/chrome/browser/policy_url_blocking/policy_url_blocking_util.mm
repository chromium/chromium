// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/policy_url_blocking_util.h"

#import <Foundation/Foundation.h>

#import "ios/net/protocol_handler_util.h"
#import "net/base/net_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy_url_blocking_util {

NSError* CreateBlockedUrlError() {
  return [NSError errorWithDomain:net::kNSErrorDomain
                             code:net::ERR_BLOCKED_BY_ADMINISTRATOR
                         userInfo:nil];
}

}  // namespace policy_url_blocking_util
