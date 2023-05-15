// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/proto_util.h"

#import "ios/web/public/session/proto/navigation.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

using NavigationProtoUtilTest = PlatformTest;

// Tests that web::ReferrerPolicy round trip correctly when serialized to proto.
TEST_F(NavigationProtoUtilTest, ReferrerPolicyRoundTrip) {
  static const ReferrerPolicy kReferrerPolicies[] = {
      ReferrerPolicyAlways,
      ReferrerPolicyDefault,
      ReferrerPolicyNoReferrerWhenDowngrade,
      ReferrerPolicyNever,
      ReferrerPolicyOrigin,
      ReferrerPolicyOriginWhenCrossOrigin,
      ReferrerPolicySameOrigin,
      ReferrerPolicyStrictOrigin,
      ReferrerPolicyStrictOriginWhenCrossOrigin,
  };

  for (ReferrerPolicy policy : kReferrerPolicies) {
    EXPECT_EQ(policy, ReferrerPolicyFromProto(ReferrerPolicyToProto(policy)));
  }
}

// Tests that web::Referrer round trip correctly when serialized to proto.
TEST_F(NavigationProtoUtilTest, ReferrerRoundTrip) {
  const Referrer referrer(GURL("https://example.com/referrer"),
                          ReferrerPolicyDefault);

  proto::ReferrerStorage storage;
  SerializeReferrerToProto(referrer, storage);

  EXPECT_EQ(referrer, ReferrerFromProto(storage));
}

// Tests that HttpRequestHeaders round trip correctly when serialized to proto.
TEST_F(NavigationProtoUtilTest, HttpRequestHeadersRoundTrip) {
  NSDictionary<NSString*, NSString*>* http_request_headers = @{
    @"key1" : @"value1",
    @"key2" : @"value2",
  };

  proto::HttpHeaderListStorage storage;
  SerializeHttpRequestHeadersToProto(http_request_headers, storage);

  EXPECT_NSEQ(http_request_headers, HttpRequestHeadersFromProto(storage));
}

}  // anonymous namespace
}  // namespace web
