// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/proto_util.h"

#import <ostream>
#import <type_traits>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ReferrerPolicy ReferrerPolicyFromProto(proto::ReferrerPolicy value) {
  switch (value) {
    case proto::ReferrerPolicy::Always:
      return ReferrerPolicyAlways;

    case proto::ReferrerPolicy::Default:
      return ReferrerPolicyDefault;

    case proto::ReferrerPolicy::NoReferrerWhenDowngrade:
      return ReferrerPolicyNoReferrerWhenDowngrade;

    case proto::ReferrerPolicy::Never:
      return ReferrerPolicyNever;

    case proto::ReferrerPolicy::Origin:
      return ReferrerPolicyOrigin;

    case proto::ReferrerPolicy::OriginWhenCrossOrigin:
      return ReferrerPolicyOriginWhenCrossOrigin;

    case proto::ReferrerPolicy::SameOrigin:
      return ReferrerPolicySameOrigin;

    case proto::ReferrerPolicy::StrictOrigin:
      return ReferrerPolicyStrictOrigin;

    case proto::ReferrerPolicy::StrictOriginWhenCrossOrigin:
      return ReferrerPolicyStrictOriginWhenCrossOrigin;

    default:
      NOTREACHED_NORETURN()
          << "Invalid web::proto::ReferrerPolicy: "
          << static_cast<std::underlying_type<proto::ReferrerPolicy>::type>(
                 value);
  }
}

proto::ReferrerPolicy ReferrerPolicyToProto(ReferrerPolicy value) {
  switch (value) {
    case ReferrerPolicyAlways:
      return proto::ReferrerPolicy::Always;

    case ReferrerPolicyDefault:
      return proto::ReferrerPolicy::Default;

    case ReferrerPolicyNoReferrerWhenDowngrade:
      return proto::ReferrerPolicy::NoReferrerWhenDowngrade;

    case ReferrerPolicyNever:
      return proto::ReferrerPolicy::Never;

    case ReferrerPolicyOrigin:
      return proto::ReferrerPolicy::Origin;

    case ReferrerPolicyOriginWhenCrossOrigin:
      return proto::ReferrerPolicy::OriginWhenCrossOrigin;

    case ReferrerPolicySameOrigin:
      return proto::ReferrerPolicy::SameOrigin;

    case ReferrerPolicyStrictOrigin:
      return proto::ReferrerPolicy::StrictOrigin;

    case ReferrerPolicyStrictOriginWhenCrossOrigin:
      return proto::ReferrerPolicy::StrictOriginWhenCrossOrigin;
  }

  NOTREACHED_NORETURN()
      << "Invalid web::ReferrerPolicy: "
      << static_cast<std::underlying_type<ReferrerPolicy>::type>(value);
}

Referrer ReferrerFromProto(const proto::ReferrerStorage& storage) {
  return Referrer(GURL(storage.url()),
                  ReferrerPolicyFromProto(storage.policy()));
}

void SerializeReferrerToProto(const Referrer& referrer,
                              proto::ReferrerStorage& storage) {
  CHECK(referrer.url.is_valid());
  storage.set_url(referrer.url.spec());
  storage.set_policy(ReferrerPolicyToProto(referrer.policy));
}

NSMutableDictionary<NSString*, NSString*>* HttpRequestHeadersFromProto(
    const proto::HttpHeaderListStorage& storage) {
  NSMutableDictionary<NSString*, NSString*>* headers =
      [[NSMutableDictionary alloc] initWithCapacity:storage.headers_size()];

  for (const proto::HttpHeaderStorage& header : storage.headers()) {
    NSString* key = base::SysUTF8ToNSString(header.name());
    NSString* val = base::SysUTF8ToNSString(header.value());

    headers[key] = val;
  }

  return headers;
}

void SerializeHttpRequestHeadersToProto(
    NSDictionary<NSString*, NSString*>* headers,
    proto::HttpHeaderListStorage& storage) {
  CHECK_NE(headers.count, 0u);

  for (NSString* key in headers) {
    proto::HttpHeaderStorage* header = storage.add_headers();
    header->set_name(base::SysNSStringToUTF8(key));
    header->set_value(base::SysNSStringToUTF8(headers[key]));
  }
}

}  // namespace web
