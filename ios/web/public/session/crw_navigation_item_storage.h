// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_NAVIGATION_ITEM_STORAGE_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_NAVIGATION_ITEM_STORAGE_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <string>

#include "base/time/time.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/navigation/referrer.h"
#include "url/gurl.h"

namespace web {
namespace proto {
class NavigationItemStorage;
}  // namespace proto

// Keys used to serialize navigation properties.

// Current URL (std::string).
extern NSString* const kNavigationItemStorageURLKey;
// Current URL (std::string).
extern NSString* const kNavigationItemStorageVirtualURLKey;
// Page referrer URL (std::string).
extern NSString* const kNavigationItemStorageReferrerURLKey;
// Page referrer URL (NSURL). Deprecated, used for backward compatibility.
// TODO(crbug.com/41304278): Remove this key.
extern NSString* const kNavigationItemStorageReferrerURLDeprecatedKey;
// Page referrer policy (int).
extern NSString* const kNavigationItemStorageReferrerPolicyKey;
// The time at which the last known local navigation has completed (int64_t).
extern NSString* const kNavigationItemStorageTimestampKey;
// Page title (NSString).
extern NSString* const kNavigationItemStorageTitleKey;
// HTTP request headers (NSDictionary).
extern NSString* const kNavigationItemStorageHTTPRequestHeadersKey;
// Whether or not to bypass showing the resubmit data confirmation when loading
// a POST request (BOOL).
extern NSString* const kNavigationItemStorageSkipRepostFormConfirmationKey;
// The user agent type (std::string).
extern NSString* const kNavigationItemStorageUserAgentTypeKey;

}  // namespace web

// NSCoding-compliant class used to serialize NavigationItem's persisted
// properties.
@interface CRWNavigationItemStorage : NSObject <NSCoding>

@property(nonatomic, assign) const GURL& URL;
@property(nonatomic, assign) const GURL& virtualURL;
@property(nonatomic, assign) web::Referrer referrer;
@property(nonatomic, assign) base::Time timestamp;
@property(nonatomic, assign) const std::u16string& title;
@property(nonatomic, assign) web::UserAgentType userAgentType;
@property(nonatomic, copy)
    NSDictionary<NSString*, NSString*>* HTTPRequestHeaders;

// Convenience initializer that creates an instance from proto representation.
- (instancetype)initWithProto:(const web::proto::NavigationItemStorage&)storage;

// Serializes the CRWNavigationItemStorage into `storage`.
- (void)serializeToProto:(web::proto::NavigationItemStorage&)storage;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_NAVIGATION_ITEM_STORAGE_H_
