// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_CRW_NAVIGATION_ITEM_STORAGE_H_
#define IOS_WEB_PUBLIC_SESSION_CRW_NAVIGATION_ITEM_STORAGE_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/ui/page_display_state.h"
#include "url/gurl.h"

namespace web {

// Keys used to serialize navigation properties.

// Current URL (std::string).
extern NSString* const kNavigationItemStorageURLKey;
// Page referrer URL (std::string).
extern NSString* const kNavigationItemStorageReferrerURLKey;
// Page referrer URL (NSURL). Deprecated, used for backward compatibility.
// TODO(crbug.com/696125): Remove this key.
extern NSString* const kNavigationItemStorageReferrerURLDeprecatedKey;
// Page referrer policy (int).
extern NSString* const kNavigationItemStorageReferrerPolicyKey;
// The time at which the last known local navigation has completed (int64_t).
extern NSString* const kNavigationItemStorageTimestampKey;
// Page title (NSString).
extern NSString* const kNavigationItemStorageTitleKey;
// The PageDisplayState (NSDictionary).
extern NSString* const kNavigationItemStoragePageDisplayStateKey;
// POST request data (NSData).
extern NSString* const kNavigationItemStoragePOSTDataKey;
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

@property(nonatomic, assign) GURL virtualURL;
@property(nonatomic, assign) web::Referrer referrer;
@property(nonatomic, assign) base::Time timestamp;
@property(nonatomic, assign) base::string16 title;
@property(nonatomic, assign) web::PageDisplayState displayState;
@property(nonatomic, assign) BOOL shouldSkipRepostFormConfirmation;
@property(nonatomic, assign) web::UserAgentType userAgentType;
@property(nonatomic, copy) NSData* POSTData;
@property(nonatomic, copy) NSDictionary* HTTPRequestHeaders;

@end

#endif  // IOS_WEB_PUBLIC_SESSION_CRW_NAVIGATION_ITEM_STORAGE_H_
