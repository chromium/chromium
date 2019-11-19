// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_navigation_item_storage.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/public/web_client.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Keys used to serialize navigation properties.
NSString* const kNavigationItemStorageURLKey = @"virtualUrlString";
NSString* const kNavigationItemStorageReferrerURLKey = @"referrerUrlString";
NSString* const kNavigationItemStorageReferrerURLDeprecatedKey = @"referrer";
NSString* const kNavigationItemStorageReferrerPolicyKey = @"referrerPolicy";
NSString* const kNavigationItemStorageTimestampKey = @"timestamp";
NSString* const kNavigationItemStorageTitleKey = @"title";
NSString* const kNavigationItemStoragePageDisplayStateKey = @"state";
NSString* const kNavigationItemStoragePOSTDataKey = @"POSTData";
NSString* const kNavigationItemStorageHTTPRequestHeadersKey = @"httpHeaders";
NSString* const kNavigationItemStorageSkipRepostFormConfirmationKey =
    @"skipResubmitDataConfirmation";
NSString* const kNavigationItemStorageUserAgentTypeKey = @"userAgentType";

}  // namespace web

@implementation CRWNavigationItemStorage

@synthesize virtualURL = _virtualURL;
@synthesize referrer = _referrer;
@synthesize timestamp = _timestamp;
@synthesize title = _title;
@synthesize displayState = _displayState;
@synthesize shouldSkipRepostFormConfirmation =
    _shouldSkipRepostFormConfirmation;
@synthesize userAgentType = _userAgentType;
@synthesize POSTData = _POSTData;
@synthesize HTTPRequestHeaders = _HTTPRequestHeaders;

#pragma mark - NSObject

- (NSString*)description {
  NSMutableString* description =
      [NSMutableString stringWithString:[super description]];
  [description appendFormat:@"virtualURL : %s, ", _virtualURL.spec().c_str()];
  [description appendFormat:@"referrer : %s, ", _referrer.url.spec().c_str()];
  [description appendFormat:@"timestamp : %f, ", _timestamp.ToCFAbsoluteTime()];
  [description appendFormat:@"title : %@, ", base::SysUTF16ToNSString(_title)];
  [description
      appendFormat:@"displayState : %@", _displayState.GetDescription()];
  [description appendFormat:@"skipRepostConfirmation : %@, ",
                            @(_shouldSkipRepostFormConfirmation)];
  [description
      appendFormat:@"userAgentType : %s, ",
                   web::GetUserAgentTypeDescription(_userAgentType).c_str()];
  [description appendFormat:@"POSTData : %@, ", _POSTData];
  [description appendFormat:@"HTTPRequestHeaders : %@", _HTTPRequestHeaders];
  return description;
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    // Desktop chrome only persists virtualUrl_ and uses it to feed the url
    // when creating a NavigationEntry.
    if ([aDecoder containsValueForKey:web::kNavigationItemStorageURLKey]) {
      _virtualURL = GURL(web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageURLKey));
    }

    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageReferrerURLKey]) {
      const std::string referrerString(web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageReferrerURLKey));
      web::ReferrerPolicy referrerPolicy =
          static_cast<web::ReferrerPolicy>([aDecoder
              decodeIntForKey:web::kNavigationItemStorageReferrerPolicyKey]);
      _referrer = web::Referrer(GURL(referrerString), referrerPolicy);
    } else {
      // Backward compatibility.
      NSURL* referrerURL =
          [aDecoder decodeObjectForKey:
                        web::kNavigationItemStorageReferrerURLDeprecatedKey];
      _referrer = web::Referrer(net::GURLWithNSURL(referrerURL),
                                web::ReferrerPolicyDefault);
    }

    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageTimestampKey]) {
      int64_t us =
          [aDecoder decodeInt64ForKey:web::kNavigationItemStorageTimestampKey];
      _timestamp = base::Time::FromInternalValue(us);
    }

    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageUserAgentTypeKey]) {
      std::string userAgentDescription = web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageUserAgentTypeKey);
      _userAgentType =
          web::GetUserAgentTypeWithDescription(userAgentDescription);
    } else if (web::GetWebClient()->IsAppSpecificURL(_virtualURL)) {
      // Legacy CRWNavigationItemStorages didn't have the concept of a NONE
      // user agent for app-specific URLs, so check decoded virtual URL before
      // attempting to decode the deprecated key.
      _userAgentType = web::UserAgentType::NONE;
    }

    NSString* title =
        [aDecoder decodeObjectForKey:web::kNavigationItemStorageTitleKey];
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.  This is what desktop chrome does.
    _title = base::SysNSStringToUTF16(title);
    NSDictionary* serializedDisplayState = [aDecoder
        decodeObjectForKey:web::kNavigationItemStoragePageDisplayStateKey];
    _displayState = web::PageDisplayState(serializedDisplayState);
    _shouldSkipRepostFormConfirmation =
        [aDecoder decodeBoolForKey:
                      web::kNavigationItemStorageSkipRepostFormConfirmationKey];
    _POSTData =
        [aDecoder decodeObjectForKey:web::kNavigationItemStoragePOSTDataKey];
    _HTTPRequestHeaders = [aDecoder
        decodeObjectForKey:web::kNavigationItemStorageHTTPRequestHeadersKey];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  // Desktop Chrome doesn't persist |url_| or |originalUrl_|, only
  // |virtualUrl_|.
  web::nscoder_util::EncodeString(aCoder, web::kNavigationItemStorageURLKey,
                                  _virtualURL.spec());
  web::nscoder_util::EncodeString(
      aCoder, web::kNavigationItemStorageReferrerURLKey, _referrer.url.spec());
  [aCoder encodeInt:_referrer.policy
             forKey:web::kNavigationItemStorageReferrerPolicyKey];
  [aCoder encodeInt64:_timestamp.ToInternalValue()
               forKey:web::kNavigationItemStorageTimestampKey];

  [aCoder encodeObject:base::SysUTF16ToNSString(_title)
                forKey:web::kNavigationItemStorageTitleKey];
  [aCoder encodeObject:_displayState.GetSerialization()
                forKey:web::kNavigationItemStoragePageDisplayStateKey];
  [aCoder encodeBool:_shouldSkipRepostFormConfirmation
              forKey:web::kNavigationItemStorageSkipRepostFormConfirmationKey];
  web::nscoder_util::EncodeString(
      aCoder, web::kNavigationItemStorageUserAgentTypeKey,
      web::GetUserAgentTypeDescription(_userAgentType));
  [aCoder encodeObject:_POSTData forKey:web::kNavigationItemStoragePOSTDataKey];
  [aCoder encodeObject:_HTTPRequestHeaders
                forKey:web::kNavigationItemStorageHTTPRequestHeadersKey];
}

@end
