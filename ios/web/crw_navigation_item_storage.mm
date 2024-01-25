// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_navigation_item_storage.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/navigation/proto_util.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/web_client.h"
#import "net/base/apple/url_conversions.h"

namespace web {

// Keys used to serialize navigation properties.
NSString* const kNavigationItemStorageURLKey = @"urlString";
NSString* const kNavigationItemStorageVirtualURLKey = @"virtualUrlString";
NSString* const kNavigationItemStorageReferrerURLKey = @"referrerUrlString";
NSString* const kNavigationItemStorageReferrerURLDeprecatedKey = @"referrer";
NSString* const kNavigationItemStorageReferrerPolicyKey = @"referrerPolicy";
NSString* const kNavigationItemStorageTimestampKey = @"timestamp";
NSString* const kNavigationItemStorageTitleKey = @"title";
NSString* const kNavigationItemStorageHTTPRequestHeadersKey = @"httpHeaders";
NSString* const kNavigationItemStorageUserAgentTypeKey = @"userAgentType";

}  // namespace web

@implementation CRWNavigationItemStorage {
  GURL _URL;
  GURL _virtualURL;
  std::u16string _title;
}

- (instancetype)initWithProto:
    (const web::proto::NavigationItemStorage&)storage {
  if ((self = [super init])) {
    _URL = GURL(storage.url());
    _virtualURL = GURL(storage.virtual_url());
    _title = base::UTF8ToUTF16(storage.title());
    _timestamp = web::TimeFromProto(storage.timestamp());
    _userAgentType = web::UserAgentTypeFromProto(storage.user_agent());
    _referrer = web::ReferrerFromProto(storage.referrer());
    _HTTPRequestHeaders =
        web::HttpRequestHeadersFromProto(storage.http_request_headers());
  }
  return self;
}

- (void)serializeToProto:(web::proto::NavigationItemStorage&)storage {
  if (_URL.is_valid()) {
    storage.set_url(_URL.spec());
  }
  if (_virtualURL.is_valid()) {
    storage.set_virtual_url(_virtualURL.spec());
  }
  storage.set_title(base::UTF16ToUTF8(_title));
  web::SerializeTimeToProto(_timestamp, *storage.mutable_timestamp());
  storage.set_user_agent(web::UserAgentTypeToProto(_userAgentType));
  // To reduce disk usage, NavigationItemImpl does not serialize invalid
  // referrer or empty HTTP header map. The helper function responsible
  // for the serialisation enforces this with assertion, so skip items
  // that should not be serialised.
  if (_referrer.url.is_valid()) {
    web::SerializeReferrerToProto(_referrer, *storage.mutable_referrer());
  }
  if (_HTTPRequestHeaders.count) {
    web::SerializeHttpRequestHeadersToProto(
        _HTTPRequestHeaders, *storage.mutable_http_request_headers());
  }
}

#pragma mark - NSObject

- (BOOL)isEqual:(NSObject*)object {
  CRWNavigationItemStorage* other =
      base::apple::ObjCCast<CRWNavigationItemStorage>(object);

  return [other cr_isEqualSameClass:self];
}

- (NSString*)description {
  NSMutableString* description =
      [NSMutableString stringWithString:[super description]];
  [description appendFormat:@"URL : %s, ", _URL.spec().c_str()];
  [description appendFormat:@"virtualURL : %s, ", _virtualURL.spec().c_str()];
  [description appendFormat:@"referrer : %s, ", _referrer.url.spec().c_str()];
  [description appendFormat:@"timestamp : %f, ", _timestamp.ToCFAbsoluteTime()];
  [description appendFormat:@"title : %@, ", base::SysUTF16ToNSString(_title)];
  [description
      appendFormat:@"userAgentType : %s, ",
                   web::GetUserAgentTypeDescription(_userAgentType).c_str()];
  [description appendFormat:@"HTTPRequestHeaders : %@", _HTTPRequestHeaders];
  return description;
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    // Desktop chrome only persists virtualUrl_ and uses it to feed the url
    // when creating a NavigationEntry. Chrome on iOS is also storing _url.
    if ([aDecoder
            containsValueForKey:web::kNavigationItemStorageVirtualURLKey]) {
      _virtualURL = GURL(web::nscoder_util::DecodeString(
          aDecoder, web::kNavigationItemStorageVirtualURLKey));
    }

    if ([aDecoder containsValueForKey:web::kNavigationItemStorageURLKey]) {
      _URL = GURL(web::nscoder_util::DecodeString(
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
    _HTTPRequestHeaders = [aDecoder
        decodeObjectForKey:web::kNavigationItemStorageHTTPRequestHeadersKey];
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  // Desktop Chrome doesn't persist `url_` or `originalUrl_`, only
  // `virtualUrl_`. Chrome on iOS is persisting `url_`.
  if (_virtualURL != _URL && _virtualURL.is_valid()) {
    // In most cases _virtualURL is the same as URL. Not storing virtual URL
    // will save memory during unarchiving.
    const std::string& virtualURLSpec = _virtualURL.spec();
    web::nscoder_util::EncodeString(
        aCoder, web::kNavigationItemStorageVirtualURLKey, virtualURLSpec);
  }

  if (_URL.is_valid()) {
    web::nscoder_util::EncodeString(aCoder, web::kNavigationItemStorageURLKey,
                                    _URL.spec());
  }

  if (_referrer.url.is_valid()) {
    web::nscoder_util::EncodeString(aCoder,
                                    web::kNavigationItemStorageReferrerURLKey,
                                    _referrer.url.spec());
  }

  [aCoder encodeInt:_referrer.policy
             forKey:web::kNavigationItemStorageReferrerPolicyKey];
  [aCoder encodeInt64:_timestamp.ToInternalValue()
               forKey:web::kNavigationItemStorageTimestampKey];
  [aCoder encodeObject:base::SysUTF16ToNSString(_title)
                forKey:web::kNavigationItemStorageTitleKey];
  web::nscoder_util::EncodeString(
      aCoder, web::kNavigationItemStorageUserAgentTypeKey,
      web::GetUserAgentTypeDescription(_userAgentType));
  [aCoder encodeObject:_HTTPRequestHeaders
                forKey:web::kNavigationItemStorageHTTPRequestHeadersKey];
}

#pragma mark - Properties

- (const GURL&)URL {
  return _URL;
}

- (void)setURL:(const GURL&)URL {
  _URL = URL;
}

- (const GURL&)virtualURL {
  // virtualURL is not stored (see -encodeWithCoder:) if it's the same as URL.
  // This logic repeats NavigationItemImpl::GetURL to store virtualURL only when
  // different from URL.
  return _virtualURL.is_empty() ? _URL : _virtualURL;
}

- (void)setVirtualURL:(const GURL&)virtualURL {
  _virtualURL = virtualURL;
}

- (const std::u16string&)title {
  return _title;
}

- (void)setTitle:(const std::u16string&)title {
  _title = title;
}

#pragma mark Private

- (BOOL)cr_isEqualSameClass:(CRWNavigationItemStorage*)other {
  if (_URL != other.URL) {
    return NO;
  }

  // -virtualURL getter is complex and does not always return `_virtualURL`,
  // so use the property for both `self` and `other` to ensure correctness.
  if (self.virtualURL != other.virtualURL) {
    return NO;
  }

  if (_referrer != other.referrer) {
    return NO;
  }

  if (_timestamp != other.timestamp) {
    return NO;
  }

  if (_title != other.title) {
    return NO;
  }

  if (_userAgentType != other.userAgentType) {
    return NO;
  }

  if (_HTTPRequestHeaders != other.HTTPRequestHeaders &&
      ![_HTTPRequestHeaders isEqual:other.HTTPRequestHeaders]) {
    return NO;
  }

  return YES;
}

@end
