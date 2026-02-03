// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_util.h"

#import <Foundation/Foundation.h>

#import "ios/net/protocol_handler_util.h"
#import "net/base/net_errors.h"

namespace policy_url_blocking_util {

NSError* CreateBlockedUrlError(
    PolicyBlocklistService::PolicyBlocklistState::PolicySource policy_source) {
  net::Error err_code =
      policy_source ==
              PolicyBlocklistService::PolicyBlocklistState::INCOGNITO_POLICY
          ? net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR
          : net::ERR_BLOCKED_BY_ADMINISTRATOR;
  return [NSError errorWithDomain:net::kNSErrorDomain
                             code:err_code
                         userInfo:nil];
}

}  // namespace policy_url_blocking_util
